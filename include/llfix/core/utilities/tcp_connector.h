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

#include <cassert>
#include <cstddef>
#include <string_view>

#include "../compiler/hints_branch_predictor.h"
#include "../compiler/builtin_functions.h"
#include "../compiler/hints_hot_code.h"

#include "../os/socket.h"
#include "../os/select.h"

#include "allocator.h"
#include "tcp_connector_options.h"

namespace llfix
{

class TCPConnector
{
public:

    void set_params(const TCPConnectorOptions& options)
    {
        m_options = options;
    }

    TCPConnector()
    {
        m_rx_buffer = nullptr;
        m_incomplete_buffer = nullptr;
    }

    virtual ~TCPConnector()
    {
        close();

        if(m_rx_buffer)
        {
            Allocator::deallocate(m_rx_buffer, m_options.m_rx_buffer_capacity);
            m_rx_buffer = nullptr;
        }

        if(m_incomplete_buffer)
        {
            Allocator::deallocate(m_incomplete_buffer, m_options.m_rx_buffer_capacity * 2);
            m_incomplete_buffer = nullptr;
        }
    }

    bool connect(const std::string_view& target_address, int port)
    {
        if(m_options.m_rx_buffer_capacity == 0 || target_address.length() == 0 || port <= 0)
            return false;

        if(m_rx_buffer == nullptr)
        {
            m_rx_buffer = static_cast<char*>(Allocator::allocate(m_options.m_rx_buffer_capacity));

            if (m_rx_buffer == nullptr)
            {
                return false;
            }
        }

        if(m_incomplete_buffer == nullptr)
        {
            m_incomplete_buffer = static_cast<char*>(Allocator::allocate(m_options.m_rx_buffer_capacity * 2));

            if (m_incomplete_buffer == nullptr)
            {
                return false;
            }
        }

        close();

        if (m_socket.create() == false)
        {
            return false;
        }

        m_socket.set_socket_option(SocketOptionLevel::TCP, SocketOption::TCP_DISABLE_NAGLE, m_options.m_disable_nagle ? 1 : 0);
        m_socket.set_socket_option(SocketOptionLevel::TCP, SocketOption::TCP_ENABLE_QUICKACK, m_options.m_enable_quick_ack ? 1 : 0);
        m_socket.set_socket_option(SocketOptionLevel::SOCKET, SocketOption::RECEIVE_BUFFER_SIZE, m_options.m_socket_rx_size);
        m_socket.set_socket_option(SocketOptionLevel::SOCKET, SocketOption::SEND_BUFFER_SIZE, m_options.m_socket_tx_size);

        if(m_options.m_busy_poll_microseconds > 0)
        {
            m_socket.set_socket_option(SocketOptionLevel::SOCKET, SocketOption::BUSY_POLL_MICROSECONDS, m_options.m_busy_poll_microseconds);
        }

        m_socket.set_blocking_mode(false);

        m_asio_reader.initialise(m_options.m_async_io_timeout_nanoseconds);

        if (m_options.m_nic_interface_name.length() > 0)
        {
            if (m_socket.bind(m_options.m_nic_interface_name, 0) == false)
            {
                return false;
            }
        }

        m_asio_reader.clear_descriptors();
        m_asio_reader.add_descriptor(m_socket.get_socket_descriptor());

        m_socket.set_blocking_mode(true);
        bool result = false;

        if (m_socket.connect(target_address, port) == true)
        {
            result = true;
        }

        m_socket.set_blocking_mode(false);

        return result;
    }

    void process_incoming_messages()
    {
        int result = 0;

        result = m_asio_reader.get_number_of_ready_descriptors();

        if (result > 0)
        {
            if (m_asio_reader.is_descriptor_ready(m_socket.get_socket_descriptor()))
            {
                on_data_ready();
            }
        }
        else if (result != 0)  // 0 means timeout
        {
            auto error_code = Socket<SocketType::TCP>::get_current_thread_last_socket_error();
            on_async_io_error(error_code, result);
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // TX
    LLFIX_FORCE_INLINE bool send(const char* buffer, std::size_t buffer_size)
    {
        int bytes_sent{0};
        int iteration{0};

        while (true)
        {
            if(m_options.m_send_try_count>0)
            {
                if(iteration == m_options.m_send_try_count)
                {
                    return false;
                }

                iteration++;
            }

            auto result = m_socket.send(buffer + bytes_sent, buffer_size - bytes_sent);

            if (result > 0 && result <= static_cast<int>(buffer_size))
            {
                bytes_sent += result;
            }
            else
            {
                auto error = Socket<SocketType::TCP>::get_current_thread_last_socket_error();

                if (m_socket.is_connection_lost_during_send(error))
                {
                    on_connection_lost();
                    return false;
                }
                else
                {
                    on_socket_error(error);
                }
            }

            if (static_cast<std::size_t>(bytes_sent) == buffer_size)
            {
                return true;
            }
        }
    }
    ///////////////////////////////////////////////////////////////////////
    // RX
    LLFIX_FORCE_INLINE int receive()
    {
        m_last_receive_result = m_socket.receive(m_rx_buffer, m_options.m_receive_size);
        return m_last_receive_result;
    }

    // We provide this api in order to have the same interface with low latency NIC apis
    LLFIX_FORCE_INLINE void receive_done()
    {
        if( m_last_receive_result <= 0 )
        {
            check_errors_during_receive();
        }
    }

    LLFIX_FORCE_INLINE char* get_rx_buffer()
    {
        if(llfix_likely(m_incomplete_buffer_size==0))
        {
            return m_rx_buffer;
        }
        else
        {
            llfix_builtin_memcpy(m_incomplete_buffer+m_incomplete_buffer_size, m_rx_buffer, m_last_receive_result);
            return m_incomplete_buffer;
        }
    }

    LLFIX_FORCE_INLINE std::size_t get_rx_buffer_size()
    {
        return m_last_receive_result + m_incomplete_buffer_size;
    }

    LLFIX_FORCE_INLINE std::size_t get_rx_buffer_capacity()
    {
        return m_options.m_rx_buffer_capacity;
    }

    LLFIX_FORCE_INLINE void set_incomplete_buffer(char* buffer, std::size_t buffer_size)
    {
        assert(buffer != nullptr && buffer_size > 0);
        assert(buffer_size <= (m_options.m_rx_buffer_capacity*2));
        if(buffer != m_incomplete_buffer)
            llfix_builtin_memcpy(m_incomplete_buffer, buffer, buffer_size);
        m_incomplete_buffer_size = buffer_size;
    }

    LLFIX_FORCE_INLINE void reset_incomplete_buffer()
    {
        m_incomplete_buffer_size = 0;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual void on_data_ready() = 0;
    virtual void on_connection_lost() = 0;
    virtual void on_socket_error(int error_code) = 0;
    virtual void on_async_io_error(int error_code, int event_result) = 0;
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    #ifdef LLFIX_UNIT_TEST
    std::size_t get_incomplete_buffer_size() const { return m_incomplete_buffer_size; }
    #endif

protected:
    TCPConnectorOptions m_options;

    Socket<SocketType::TCP> m_socket;
    Select m_asio_reader;

    int m_last_receive_result = 0;

    char* m_rx_buffer = nullptr;

    char* m_incomplete_buffer = nullptr;
    std::size_t m_incomplete_buffer_size = 0;

    void check_errors_during_receive()
    {
        auto error = Socket<SocketType::TCP>::get_current_thread_last_socket_error();

        if (error == 0)
        {
            on_connection_lost();
        }
        else if (m_socket.is_connection_lost_during_receive(error))
        {
            on_connection_lost();
        }
        else
        {
            on_socket_error(error);
        }
    }

    void close()
    {
        if(m_rx_buffer)
            llfix_builtin_memset(m_rx_buffer, 0, m_options.m_rx_buffer_capacity);

        if(m_incomplete_buffer)
            llfix_builtin_memset(m_incomplete_buffer, 0, m_options.m_rx_buffer_capacity*2);

        m_socket.close();
        m_asio_reader.clear_descriptors();

        m_last_receive_result = 0;
        m_incomplete_buffer_size = 0;
    }
};

} // namespace