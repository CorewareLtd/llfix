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
#include <cstdint>
#include <cstddef>
#include <string>

#include "../core/compiler/unused.h"
#include "../core/compiler/builtin_functions.h"

#include "../core/os/epoll.h"

#include "../electronic_trading/managed_instance/managed_instance.h"

#include "../core/utilities/tcp_reactor.h"
#include "../core/utilities/logger.h"

#include "management_context.h"
#include "management_server_settings.h"
#include "commands.h"
#include "command_factory.h"

namespace llfix
{

/**
 * @brief Management TCP server for FIX engine runtime control and inspection.
 *
 * @details
 * ManagementServer provides a lightweight, delimiter-based TCP interface used
 * to manage and introspect FIX clients and FIX servers at runtime.
 *
 * It is **not** a Telnet-compatible server and accepts only plain TCP connections.
 * Commands and responses are delimited using `Commands::COMMAND_DELIMITER` (e.g. '|').
 *
 * The server supports:
 *  - Runtime registration of managed FIX client and server instances
 *  - Execution of management commands via CommandFactory
 *  - Centralised access to engine metadata (version, start time, log path)
 *
 * @note
 * The server must be successfully created via create() before registering
 * any managed instances.
 *
 * @warning
 * Instance names must be unique across both clients and servers.
 */
class ManagementServer : public TcpReactor<Epoll>
{
    public:

        ManagementServer() = default;
        virtual ~ManagementServer() = default;

        bool create(const ManagementServerSettings& settings, uint64_t application_start_time_timestamp, const std::string& engine_version, const std::string& log_file_path)
        {
            if(settings.validate() == false)
            {
                return false;
            }

            m_options.m_nic_interface_ip = settings.management_server_nic_ip;
            m_options.m_port = settings.management_server_port;
            m_options.m_cpu_core_id = settings.management_server_cpu_core_id;
            set_params(m_options);

            if (start() == false)
            {
                LLFIX_LOG_ERROR("Management server failed to start");
                return false;
            }

            m_management_context.application_start_timestamp = application_start_time_timestamp;
            m_management_context.engine_version = engine_version;
            m_management_context.log_file_path = log_file_path;

            LLFIX_LOG_INFO(std::string("Management server settings => ") + settings.to_string());

            m_successfully_created = true;
            return true;
        }

        /**
         * @brief Registers a managed FIX client instance with the management server.
         *
         * @details
         * Adds the provided ManagedInstance to the internal client registry,
         * making it accessible to management commands
         *
         * The management server must be successfully initialised prior to calling this method.
         *
         * @param instance Pointer to a ManagedInstance representing a FIX client.
         *
         * @return
         * true if the client was successfully registered, false otherwise.
         *
         * @note
         * Client instance names must be unique across all registered clients and servers.
         */
        bool register_client(ManagedInstance* instance)
        {
            assert(instance);

            if (!m_successfully_created)
            {
                LLFIX_LOG_ERROR("register_client called however management server did not initialise correctly. Please check the configs and logs");
                return false;
            }

            if(m_management_context.has_instance(instance->get_name()))
            {
                LLFIX_LOG_ERROR("You need to use unique instance names");
                return false;
            }

            m_management_context.client_instances.push_back(instance);
            return true;
        }

        /**
         * @brief Registers a managed FIX server instance with the management server.
         *
         * @details
         * Adds the provided ManagedInstance to the internal server registry,
         * allowing management commands to interact with FIX server components.
         *
         * The management server must be fully initialised before invoking this method.
         *
         * @param instance Pointer to a ManagedInstance representing a FIX server.
         *
         * @return
         * true if the server was successfully registered, false otherwise.
         *
         * @note
         * Server instance names must be unique across all registered clients and servers.
         */
        bool register_server(ManagedInstance* instance)
        {
            assert(instance);

            if (!m_successfully_created)
            {
                LLFIX_LOG_ERROR("register_server called however management server did not initialise correctly. Please check the configs and logs");
                return false;
            }

            if(m_management_context.has_instance(instance->get_name()))
            {
                LLFIX_LOG_ERROR("You need to use unique instance names");
                return false;
            }

            m_management_context.server_instances.push_back(instance);
            return true;
        }

        void on_data_ready(std::size_t peer_index) override
        {
            auto read = receive(peer_index);

            if (read > 0 && read <= static_cast<int>(m_options.m_rx_buffer_capacity))
            {
                process_rx_buffer(peer_index, get_rx_buffer(peer_index), get_rx_buffer_size(peer_index));
            }

            receive_done(peer_index);
        }

        void on_client_connected(std::size_t peer_index) override
        {
            LLFIX_UNUSED(peer_index);
            m_no_connected_clients++;
        }

        void on_client_disconnected(std::size_t peer_index) override
        {
            if(m_no_connected_clients>0) m_no_connected_clients--;
            TcpReactor<Epoll>::on_client_disconnected(peer_index);
        }

        void on_async_io_error(int error_code, int event_result) override
        {
            LLFIX_UNUSED(error_code);
            LLFIX_UNUSED(event_result);
        }

        void on_socket_error(int error_code, int event_result) override
        {
            LLFIX_UNUSED(error_code);
            LLFIX_UNUSED(event_result);
        }

        int get_no_connected_clients() const { return m_no_connected_clients; }

    private:
        int m_no_connected_clients = 0;
        ManagementContext m_management_context;
        bool m_successfully_created = false;

        void process_rx_buffer(std::size_t peer_index, char* buffer, std::size_t buffer_size)
        {
            std::size_t offset = 0;
            std::size_t current_command_start = 0;
            char* current_command_buffer = nullptr;
            std::size_t buffer_read_index = 0;

            while (true)
            {
                if (buffer[offset] == Commands::COMMAND_DELIMITER)
                {
                    current_command_buffer = buffer + current_command_start;
                    on_command(current_command_buffer, offset - current_command_start, peer_index);
                    current_command_start = offset + 1;

                    buffer_read_index = offset + 1;
                }

                offset++;

                if (offset > buffer_size - 1)
                {
                    break;
                }
            }

            if (buffer_size - buffer_read_index > 0)
            {
                set_incomplete_buffer(peer_index, buffer+buffer_read_index, buffer_size - buffer_read_index);
            }
            else
            {
                reset_incomplete_buffer(peer_index);
            }
        }

        void on_command(const char* buffer, std::size_t buffer_len, std::size_t peer_index)
        {
            auto command = CommandFactory::create_command(buffer, buffer_len);

            if (command == nullptr)
            {
                on_invalid_command(peer_index, buffer, buffer_len);
                return;
            }

            auto response = command->process(m_management_context);

            if(response.length()>0)
            {
                response += Commands::COMMAND_DELIMITER;
                send(peer_index, response.c_str(), response.length());
            }

            delete command;
        }

        void on_invalid_command(std::size_t peer_index, const char* buffer, std::size_t buffer_len)
        {
            std::string response = "Invalid command";
            response += Commands::COMMAND_DELIMITER;

            send(peer_index, response.c_str(), response.length());

            static constexpr std::size_t TEMP_BUFFER_SIZE = 1024;

            char temp[TEMP_BUFFER_SIZE];

            if (TEMP_BUFFER_SIZE >= buffer_len + 1)
            {
                llfix_builtin_memcpy(temp, buffer, buffer_len);
                temp[buffer_len] = '\0';
            }
            else
            {
                llfix_builtin_memcpy(temp, buffer, TEMP_BUFFER_SIZE - 1);
                temp[TEMP_BUFFER_SIZE - 1] = '\0';
            }

            std::string error_message = "Management server invalid command : ";
            error_message += temp;
            LLFIX_LOG_ERROR(error_message);
        }
};

} // namespace