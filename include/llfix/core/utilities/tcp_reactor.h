/*
MIT License

Copyright (c) 2026 Coreware Limited

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
/*
    - Using epoll on Linux and select on Windows as their latest ioring and iouring are not widespread available

    - Concurrency : No sync needed between reactor thread and TX/RX methods as TX/RX methods will always be called from the reactor thread.
                    Sync needed between stop method and reactor thread as stop will always be called from a different thread

    - Disconnection detection : Disconnections will be detected during RX & TX.
*/
#include <atomic>
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <thread>

#include "../compiler/hints_hot_code.h"
#include "../compiler/hints_branch_predictor.h"
#include "../compiler/builtin_functions.h"
#include "../compiler/unused.h"

#include "../os/socket.h"
#include "../os/epoll.h"
#include "../os/thread_utilities.h"

#include "connectors.h"
#include "tcp_reactor_options.h"

namespace llfix
{

template <typename AsyncIOPollerType=Epoll>
class TcpReactor
{
    public:

        struct Callback
        {
            void (*fn)(void*);
            void (*deleter)(void*);
            void* ctx;
        };

        virtual ~TcpReactor()
        {
            stop();

            for (auto& cb : m_callbacks)
                cb.deleter(cb.ctx);

            for (auto& cb : m_termination_callbacks)
                cb.deleter(cb.ctx);
        }

        static constexpr bool is_multithreaded()
        {
            return false;
        }

        void set_params(const TCPReactorOptions& options)
        {
            m_options = options;
        }

        template <typename F>
        bool register_callback(F f)
        {
            try
            {
                F* context = new F(std::move(f));

                m_callbacks.push_back({
                    [](void* p) {
                        F* fn = static_cast<F*>(p);
                        (*fn)(); // invoking operator() of std::bind's ret val OR lambda expression
                    },
                    [](void* p) {
                    F* fn = static_cast<F*>(p);
                    delete fn;
                    },
                    context
                    });
            }
            catch(...)
            {
                return false;
            }

            return true;
        }

        template <typename F>
        bool register_termination_callback(F f)
        {
            try
            {
                F* context = new F(std::move(f));

                m_termination_callbacks.push_back({
                    [](void* p) {
                        F* fn = static_cast<F*>(p);
                        (*fn)(); // invoking operator() of std::bind's ret val OR lambda expression
                    },
                    [](void* p) {
                    F* fn = static_cast<F*>(p);
                    delete fn;
                    },
                    context
                    });
            }
            catch(...)
            {
                return false;
            }

            return true;
        }

        bool start()
        {
            m_is_stopping.store(false);
            m_thread_finished.store(false);

            m_connectors.initialise(m_options.m_rx_buffer_capacity);

            m_acceptor_socket.create();

            m_acceptor_socket.set_pending_connections_queue_size(m_options.m_pending_connection_queue_size);

            m_acceptor_socket.set_socket_option(SocketOptionLevel::SOCKET, SocketOption::REUSE_ADDRESS, 1);

            if(m_options.m_busy_poll_microseconds > 0)
            {
                m_acceptor_socket.set_socket_option(SocketOptionLevel::SOCKET, SocketOption::BUSY_POLL_MICROSECONDS, m_options.m_busy_poll_microseconds);
            }

            if (!m_acceptor_socket.bind(m_options.m_nic_interface_ip, m_options.m_port))
            {
                return false;
            }

            if (!m_acceptor_socket.listen())
            {
                return false;
            }

            m_acceptor_socket.set_blocking_mode(false);

            if( m_asio_reader.initialise(static_cast<long>(m_options.m_async_io_timeout_nanoseconds), m_options.m_max_poll_events) == false)
            {
                return false;
            }

            m_asio_reader.add_descriptor(m_acceptor_socket.get_socket_descriptor());

            try
            {
                m_reactor_thread.reset( new std::thread(&TcpReactor::reactor_thread, this) );
            }
            catch(...)
            {
                return false;
            }

            return true;
        }

        void stop()
        {
            if (m_is_stopping.load() == false)
            {
                m_is_stopping.store(true);

                if (m_reactor_thread.get() != nullptr)
                {
                    if( m_reactor_thread->joinable() )
                    {
                        while(true)
                        {
                            if(m_thread_finished.load() == true)
                                break;
                        }

                        m_reactor_thread->join();
                        m_asio_reader.clear_descriptors();
                        m_connectors.reset();
                        m_acceptor_socket.close();
                    }
                }
            }
        }

        bool get_peer_details(std::size_t peer_index, std::string& address, int& port)
        {
            if (!m_connectors[peer_index]->connected())
                return false;

            auto connector_socket = m_connectors[peer_index]->socket();

            if(connector_socket == nullptr)
                return false;

            auto socket_fd = connector_socket->get_socket_descriptor();
            return Socket<>::get_peer_details(socket_fd, address, port);
        }

        void close_connection(std::size_t peer_index)
        {
            on_client_disconnected(peer_index);
        }

        // Common interface
        void mark_connector_as_ready_for_worker_thread_dispatch(std::size_t connector_index)
        {
            LLFIX_UNUSED(connector_index);
        }
        ///////////////////////////////////////////////////////////////////////////////////
        // TX
        LLFIX_FORCE_INLINE bool send(std::size_t index, const char* buffer, std::size_t buffer_size)
        {
            auto peer_socket = m_connectors[index]->socket();
            int bytes_sent{0};
            int iteration{0};

            while(true)
            {
                if(m_options.m_send_try_count>0)
                {
                    if(iteration == m_options.m_send_try_count)
                    {
                        return false;
                    }

                    iteration++;
                }

                auto result = peer_socket->send(buffer + bytes_sent, buffer_size - bytes_sent);

                if ( llfix_likely (result > 0 && result <= static_cast<int>(buffer_size)) )
                {
                    bytes_sent += result;
                }
                else
                {
                    if(check_errors_during_send(index, result) == false)
                    {
                        return false;
                    }
                }

                if (static_cast<std::size_t>(bytes_sent) == buffer_size)
                {
                    return true;
                }
            }
        }
        ///////////////////////////////////////////////////////////////////////////////////
        // RX
        LLFIX_FORCE_INLINE int receive(std::size_t index)
        {
            auto connector = m_connectors[index];
            auto last_receive_result = connector->socket()->receive(connector->rx_buffer(), m_options.m_receive_size);
            connector->set_last_receive_result( last_receive_result );
            return last_receive_result;
        }

        LLFIX_FORCE_INLINE void receive_done(std::size_t index)
        {
            if( m_connectors[index]->last_receive_result() <= 0 )
            {
                check_errors_during_receive(index);
            }
        }

        LLFIX_FORCE_INLINE char* get_rx_buffer(std::size_t index)
        {
            auto connector = m_connectors[index];

            if(llfix_likely(connector->incomplete_buffer_size() ==0))
            {
                return  connector->rx_buffer();
            }
            else
            {
                llfix_builtin_memcpy(connector->incomplete_buffer()+connector->incomplete_buffer_size(), connector->rx_buffer(), connector->last_receive_result());
                return connector->incomplete_buffer();
            }
        }

        LLFIX_FORCE_INLINE void set_incomplete_buffer(std::size_t index, char* buffer, std::size_t buffer_size)
        {
            assert(buffer != nullptr && buffer_size > 0);
            assert(buffer_size <= (m_options.m_rx_buffer_capacity*2));
            if(buffer != m_connectors[index]->incomplete_buffer())
                llfix_builtin_memcpy(m_connectors[index]->incomplete_buffer(), buffer, buffer_size);
            m_connectors[index]->set_incomplete_buffer_size(buffer_size);
        }

        LLFIX_FORCE_INLINE std::size_t get_rx_buffer_size(std::size_t index)
        {
            return m_connectors[index]->last_receive_result() + m_connectors[index]->incomplete_buffer_size();
        }

        LLFIX_FORCE_INLINE void reset_incomplete_buffer(std::size_t index)
        {
            m_connectors[index]->set_incomplete_buffer_size(0);
        }

        LLFIX_FORCE_INLINE std::size_t get_rx_buffer_capacity()
        {
            return m_options.m_rx_buffer_capacity;
        }
        ///////////////////////////////////////////////////////////////////////////////////
        virtual void on_client_connected(std::size_t connector_index) = 0;

        virtual void on_client_disconnected(std::size_t connector_index)
        {
            if(m_connectors[connector_index]->socket())
                m_asio_reader.remove_descriptor(m_connectors[connector_index]->socket()->get_socket_descriptor());
            m_connectors.recycle_connector(connector_index);
        }

        virtual void on_data_ready(std::size_t connector_index) = 0;
        virtual void on_async_io_error(int error_code, int event_result) = 0;
        virtual void on_socket_error(int error_code, int event_result) = 0;
        ///////////////////////////////////////////////////////////////////////////////////

    protected:
        TCPReactorOptions m_options;
        std::unique_ptr<std::thread> m_reactor_thread;
        Socket<SocketType::TCP> m_acceptor_socket;
        AsyncIOPollerType m_asio_reader;
        std::atomic<bool> m_is_stopping = false;
        std::atomic<bool> m_thread_finished = false;

        Connectors m_connectors;
        std::vector<Callback> m_callbacks;
        std::vector<Callback> m_termination_callbacks;

    private:

        void reactor_thread()
        {
            if (m_options.m_cpu_core_id != -1)
            {
                ThreadUtilities::pin_calling_thread_to_cpu_core(m_options.m_cpu_core_id);
            }

            while (true)
            {
                if (m_is_stopping.load() == true)
                {
                    for (auto& cb : m_callbacks)
                    {
                        cb.fn(cb.ctx);
                    }

                    break;
                }

                for (auto& cb : m_callbacks)
                {
                    cb.fn(cb.ctx);
                }

                int result = 0;

                if constexpr(AsyncIOPollerType::polls_per_socket() == false)
                {
                    result = m_asio_reader.get_number_of_ready_events();
                }
                else
                {
                    result = m_asio_reader.get_number_of_ready_descriptors();
                }

                if (result > 0)
                {
                    if constexpr (AsyncIOPollerType::polls_per_socket() == false)
                    {
                        for (int counter{ 0 }; counter < result; counter++)
                        {
                            auto current_descriptor = m_asio_reader.get_ready_descriptor(counter);
                            std::size_t peer_index = m_connectors.get_connector_index_from_descriptor(current_descriptor);

                            if (m_asio_reader.is_valid_event(counter))
                            {
                                if (current_descriptor == m_acceptor_socket.get_socket_descriptor())
                                {
                                    accept_new_connection();
                                }
                                else
                                {
                                    if(m_connectors[peer_index]->connected())
                                        on_data_ready(peer_index);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (m_asio_reader.is_descriptor_ready(m_acceptor_socket.get_socket_descriptor()))
                        {
                            accept_new_connection();
                        }

                        auto peer_count = m_connectors.get_connector_count();
                        for (int counter{ 0 }; counter < static_cast<int>(peer_count); counter++)
                        {
                            if (m_connectors[counter]->connected())
                            {
                                if (m_asio_reader.is_descriptor_ready(m_connectors[counter]->socket()->get_socket_descriptor()))
                                {
                                    on_data_ready(counter);
                                }
                            }
                        }
                    }
                }
                else if (result != 0)  // 0 means timeout
                {
                    auto error_code = Socket<>::get_current_thread_last_socket_error();
                    on_async_io_error(error_code, result);
                }
            }

            for (auto& cb : m_termination_callbacks)
            {
                cb.fn(cb.ctx);
            }

            m_thread_finished.store(true);
        }

        std::size_t accept_new_connection()
        {
            std::size_t connector_index{ 0 };

            Socket<SocketType::TCP>* connector_socket = nullptr;
            connector_socket = m_acceptor_socket.accept(m_options.m_accept_timeout_seconds);

            if (connector_socket)
            {
                connector_index = m_connectors.add_connector(connector_socket);
                auto desc = connector_socket->get_socket_descriptor();
                m_asio_reader.add_descriptor(desc);
                on_client_connected(connector_index);
            }

            return connector_index;
        }

        void check_errors_during_receive(std::size_t index)
        {
            auto read = m_connectors[index]->last_receive_result();
            auto error = Socket<SocketType::TCP>::get_current_thread_last_socket_error();

            if( read == 0)
            {
                on_client_disconnected(index);
            }
            else if (m_connectors[index]->socket()->is_connection_lost_during_receive(error))
            {
                on_client_disconnected(index);
            }
            else if (error != 0)
            {
                on_socket_error(error, static_cast<int>(read));
            }
        }

        bool check_errors_during_send(std::size_t index, int send_result)
        {
            bool ok_to_continue{true};

            auto peer_socket = m_connectors[index]->socket();
            auto error = Socket<SocketType::TCP>::get_current_thread_last_socket_error();

            if (peer_socket->is_connection_lost_during_send(error))
            {
                on_client_disconnected(index);
                ok_to_continue = false;
            }
            else
            {
                on_socket_error(error, send_result);
            }

            return ok_to_continue;
        }
};

} // namespace