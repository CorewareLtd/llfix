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

#include <atomic>
#include <new>
#include <cstddef>
#include <cstdlib>
#include <vector>
#include <unordered_map>
#include <mutex> // For std::lock_guard

#include "../compiler/builtin_functions.h"
#include "../os/socket.h"
#include "userspace_spinlock.h"

namespace llfix
{

class Connector
{
    public:
        void set_socket(Socket<SocketType::TCP>* socket) { m_socket = socket; }
        void set_is_connected(bool b) { m_connected = b; }
        void set_last_receive_result(int n) { m_last_receive_result = n; }
        void set_incomplete_buffer_size(std::size_t n) { m_incomplete_buffer_size = n; }
        void set_can_be_dispatched_to_separate_thread(bool b) { m_can_be_dispatched_to_separate_thread = b; }
        void set_marked_for_recycling(bool b) { m_marked_for_recycling = b; }

        Socket<SocketType::TCP>* socket() { return m_socket; }
        bool connected() const { return m_connected; }
        int last_receive_result() const { return m_last_receive_result; }
        std::size_t incomplete_buffer_size() const { return m_incomplete_buffer_size; }
        bool can_be_dispatched_to_separate_thread() const { return m_can_be_dispatched_to_separate_thread; }
        bool marked_for_recycling() const { return m_marked_for_recycling; }

        char* incomplete_buffer() { return m_incomplete_buffer; }
        char* rx_buffer() { return m_rx_buffer; }

        Connector()
        {
            m_rx_buffer = nullptr;
            m_incomplete_buffer = nullptr;
        }

        ~Connector()
        {
            destroy();
        }

        bool initialise(std::size_t rx_buffer_capacity)
        {
            if(m_rx_buffer == nullptr)
            {
                m_rx_buffer = static_cast<char*>(malloc(rx_buffer_capacity));

                if(m_rx_buffer == nullptr)
                {
                    return false;
                }
            }
            else
            {
                llfix_builtin_memset(m_rx_buffer, 0, rx_buffer_capacity);
            }

            if(m_incomplete_buffer == nullptr)
            {
                m_incomplete_buffer = static_cast<char*>(malloc(rx_buffer_capacity*2));

                if(m_incomplete_buffer == nullptr)
                {
                    return false;
                }
            }
            else
            {
                llfix_builtin_memset(m_incomplete_buffer, 0, rx_buffer_capacity*2);
            }

            if(m_socket)
            {
                m_socket->close();
                delete m_socket;
                m_socket = nullptr;
            }

            m_marked_for_recycling = false;
            m_can_be_dispatched_to_separate_thread = false;

            return true;
        }

        void close()
        {
            m_connected = false;
            m_can_be_dispatched_to_separate_thread = false;
            m_marked_for_recycling = false;

            m_incomplete_buffer_size = 0;
            m_last_receive_result = 0;

            if(m_socket)
            {
                m_socket->close();
                delete m_socket;
                m_socket = nullptr;
            }
        }

    private:
        std::atomic<bool> m_connected = false;
        Socket<SocketType::TCP>* m_socket = nullptr;
        // RX
        int m_last_receive_result = 0;
        char* m_rx_buffer = nullptr;
        char* m_incomplete_buffer = nullptr;
        std::size_t m_incomplete_buffer_size = 0;
        std::atomic<bool> m_can_be_dispatched_to_separate_thread = false;
        std::atomic<bool> m_marked_for_recycling = false;

        void destroy()
        {
            if(m_rx_buffer != nullptr)
            {
                free(m_rx_buffer);
                m_rx_buffer = nullptr;
            }

            if(m_incomplete_buffer != nullptr)
            {
                free(m_incomplete_buffer);
                m_incomplete_buffer = nullptr;
            }

            if(m_socket)
            {
                delete m_socket;
                m_socket = nullptr;
            }

            m_incomplete_buffer_size = 0;
            m_last_receive_result = 0;

            m_connected = false;
            m_can_be_dispatched_to_separate_thread = false;
            m_marked_for_recycling = false;
        }
};

class Connectors
{
    public:

        void initialise(std::size_t rx_buffer_capacity)
        {
            m_lock.initialise();
            m_connectors.reserve(1024);
            m_descriptor_index_table.reserve(1024);
            m_rx_buffer_capacity = rx_buffer_capacity;
        }

        ~Connectors()
        {
            for (auto& connector : m_connectors)
            {
                if (connector)
                {
                    connector->close();
                    delete connector;
                }
            }
        }

        void reset()
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_lock);

            for (auto& connector : m_connectors)
            {
                if (connector)
                {
                    connector->close();
                    delete connector;
                }
            }

            m_connectors.clear();
            m_descriptor_index_table.clear();

            m_rx_buffer_capacity = 0; // per connector
        }

        std::size_t add_connector(Socket<SocketType::TCP>* connector_socket)
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_lock);

            std::size_t current_size = m_connectors.size();
            int non_used_connector_index = -1;
            std::size_t ret = -1;

            for (std::size_t i{ 0 }; i < current_size; i++)
            {
                if (m_connectors[i]->connected() == false)
                {
                    non_used_connector_index = static_cast<int>(i);
                    break;
                }
            }

            if (non_used_connector_index == -1)
            {
                // No empty slot , create new
                Connector* new_connector = new (std::nothrow) Connector;

                if(new_connector == nullptr)
                    return ret;

                if(new_connector->initialise(m_rx_buffer_capacity) == false)
                {
                    delete new_connector;
                    return ret;
                }

                new_connector->set_is_connected(true);
                new_connector->set_socket(connector_socket);

                m_connectors.push_back(new_connector);
                ret = m_connectors.size() - 1;
            }
            else
            {
                // Use an existing connector slot
                if(m_connectors[non_used_connector_index]->initialise(m_rx_buffer_capacity) == false)
                {
                    return ret;
                }

                m_connectors[non_used_connector_index]->set_socket(connector_socket);
                m_connectors[non_used_connector_index]->set_is_connected(true);
                ret = non_used_connector_index;
            }

            auto desc = connector_socket->get_socket_descriptor();
            m_descriptor_index_table[desc] = ret;

            return ret;
        }

        void recycle_connector(std::size_t connector_index)
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_lock);
            m_connectors[connector_index]->close();
        }

        std::size_t get_connector_count()
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_lock);
            return m_connectors.size();
        }

        std::size_t get_connector_index_from_descriptor(SocketFDType fd)
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_lock);
            return m_descriptor_index_table[fd];
        }

        Connector* operator[](std::size_t index)
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_lock);
            return m_connectors[index];
        }

private:
    std::vector<Connector*> m_connectors;
    std::unordered_map<SocketFDType, std::size_t> m_descriptor_index_table;
    std::size_t m_rx_buffer_capacity = 0; // per connector
    UserspaceSpinlock<> m_lock;
};

}