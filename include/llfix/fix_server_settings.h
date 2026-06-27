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

#include <cstddef>
#include <string>
#include <sstream>

#include "core/utilities/configuration.h"
#include "core/utilities/filesystem_utilities.h"
#include "core/utilities/tcp_reactor_options.h"

namespace llfix
{

struct FixServerSettings
{
    ////////////////////////////////////////////////////////////
    // THREADS
    int cpu_core_id = -1;                          // Applies to TcpReactor
    int worker_thread_count = 0;                   // Applies to TcpReactorScalable
    ////////////////////////////////////////////////////////////
    // SENDS
    int send_try_count = 0; // 0 means 'infinite'
    ////////////////////////////////////////////////////////////
    // HIGH AVAILABILITY
    bool starts_as_primary_instance = true;
    bool refresh_resend_cache_during_promotion = true;
    ////////////////////////////////////////////////////////////
    // NIC
    std::string nic_address;
    std::string nic_name;
    int nic_ringbuffer_tx_size = 2048;
    int nic_ringbuffer_rx_size = 4096;
    ////////////////////////////////////////////////////////////
    // SOCKETS
    int socket_rx_size = 212992;
    int socket_tx_size = 212992;
    ////////////////////////////////////////////////////////////
    // USERSPACE BUFFERS
    std::size_t rx_buffer_capacity = 212992;        // PER CLIENT
    std::size_t tx_encode_buffer_capacity = 212992; // PER CLIENT
    ////////////////////////////////////////////////////////////
    // TCP
    bool disable_nagle = true;
    bool quick_ack     = false;
    ////////////////////////////////////////////////////////////
    // POLLING
    int receive_size = 8192;
    long async_io_timeout_nanoseconds = TCPReactorOptions::DEFAULT_POLL_TIMEOUT_NANOSECONDS;
    int spin_count = 1;
    int busy_poll_microseconds = 0;
    std::size_t max_poll_events = 64;
    ////////////////////////////////////////////////////////////
    // OTHERS
    int accept_port = 0;
    int accept_timeout_seconds = 5;
    int pending_connection_queue_size = 32;
    ////////////////////////////////////////////////////////////
    // SSL
    #ifdef LLFIX_ENABLE_OPENSSL
    bool use_ssl = false;
    std::string ssl_version = "TLS12";
    std::string ssl_cipher_suite;
    std::string ssl_certificate_pem_file;
    std::string ssl_private_key_pem_file;
    std::string ssl_private_key_password;
    std::string ssl_ca_pem_file;
    std::string ssl_crl_path;
    bool ssl_verify_peer = true;
    #endif
    ////////////////////////////////////////////////////////////
    std::string config_load_error;
    mutable std::string validation_error;

    bool load_from_config_file(const std::string& config_file_path, const std::string& config_group_name)
    {
        Configuration config;

        if (config.load_from_file(config_file_path, config_load_error) == false)
        {
            return false;
        }

        if (config.does_group_exist(config_group_name) == false)
        {
            config_load_error = config_group_name + " does not exist";
            return false;
        }

        if (config.validate_loaded_configs({
                                            // THREADS
                                            "cpu_core_id", "worker_thread_count",
                                            // SENDS
                                            "send_try_count",
                                            // HIGH AVAILABILITY
                                            "starts_as_primary_instance", "refresh_resend_cache_during_promotion",
                                            // NIC
                                            "nic_address", "nic_name", "nic_ringbuffer_tx_size", "nic_ringbuffer_rx_size",
                                            // SOCKETS
                                            "socket_rx_size", "socket_tx_size",
                                            // USERSPACE BUFFERS
                                            "rx_buffer_capacity", "tx_encode_buffer_capacity",
                                            // TCP
                                            "disable_nagle", "quick_ack",
                                            // POLLING
                                            "receive_size", "async_io_timeout_nanoseconds","spin_count", "busy_poll_microseconds","max_poll_events",
                                            // OTHERS
                                            "accept_port","accept_timeout_seconds","pending_connection_queue_size",
                                            // SSL
                                            "use_ssl", "ssl_version", "ssl_cipher_suite", "ssl_certificate_pem_file", "ssl_private_key_pem_file", "ssl_private_key_password", "ssl_ca_pem_file", "ssl_crl_path", "ssl_verify_peer"
                                            }, config_load_error, config_group_name) == false)
        {
            return false;
        }

        // THREADS
        cpu_core_id = config.get_int_value("cpu_core_id", -1, config_group_name);
        worker_thread_count = config.get_int_value("worker_thread_count", 0, config_group_name);

        // SENDS
        send_try_count = config.get_int_value("send_try_count", 0, config_group_name);

        // HIGH AVAILABILITY
        starts_as_primary_instance = config.get_bool_value("starts_as_primary_instance", true, config_group_name);
        refresh_resend_cache_during_promotion = config.get_bool_value("refresh_resend_cache_during_promotion", true, config_group_name);

        // NIC
        nic_address = config.get_string_value("nic_address", "", config_group_name);
        nic_name = config.get_string_value("nic_name", "", config_group_name);
        nic_ringbuffer_tx_size = config.get_int_value("nic_ringbuffer_tx_size", 2048, config_group_name);
        nic_ringbuffer_rx_size = config.get_int_value("nic_ringbuffer_rx_size", 4096, config_group_name);

        // SOCKETS
        socket_rx_size = config.get_int_value("socket_rx_size", 212992, config_group_name);
        socket_tx_size = config.get_int_value("socket_tx_size", 212992, config_group_name);

        // USERSPACE BUFFERS
        rx_buffer_capacity = config.get_int_value("rx_buffer_capacity", 212992, config_group_name);
        tx_encode_buffer_capacity = config.get_int_value("tx_encode_buffer_capacity", 212992, config_group_name);

        // TCP
        disable_nagle = config.get_bool_value("disable_nagle", true, config_group_name);
        quick_ack = config.get_bool_value("quick_ack", false, config_group_name);

        // POLLING
        receive_size = config.get_int_value("receive_size", 8192, config_group_name);
        async_io_timeout_nanoseconds = config.get_int_value("async_io_timeout_nanoseconds", TCPReactorOptions::DEFAULT_POLL_TIMEOUT_NANOSECONDS, config_group_name);
        busy_poll_microseconds = config.get_int_value("busy_poll_microseconds", 0, config_group_name);
        spin_count = config.get_int_value("spin_count", 1, config_group_name);
        max_poll_events = config.get_int_value("max_poll_events", 64, config_group_name);

        // OTHERS
        accept_port = config.get_int_value("accept_port", 0, config_group_name);
        accept_timeout_seconds = config.get_int_value("accept_timeout_seconds", 5, config_group_name);
        pending_connection_queue_size = config.get_int_value("pending_connection_queue_size", 32, config_group_name);

        // SSL
        #ifdef LLFIX_ENABLE_OPENSSL
        use_ssl = config.get_bool_value("use_ssl", false, config_group_name);
        ssl_version = config.get_string_value("ssl_version", "TLS12", config_group_name);
        ssl_cipher_suite = config.get_string_value("ssl_cipher_suite", "", config_group_name);

        ssl_certificate_pem_file = config.get_string_value("ssl_certificate_pem_file", "", config_group_name);
        ssl_certificate_pem_file = FileSystemUtilities::convert_relative_path_to_absolute_path(ssl_certificate_pem_file);
        ssl_certificate_pem_file = FileSystemUtilities::normalise_path(ssl_certificate_pem_file);

        ssl_private_key_pem_file = config.get_string_value("ssl_private_key_pem_file", "", config_group_name);
        ssl_private_key_pem_file = FileSystemUtilities::convert_relative_path_to_absolute_path(ssl_private_key_pem_file);
        ssl_private_key_pem_file = FileSystemUtilities::normalise_path(ssl_private_key_pem_file);

        ssl_private_key_password = config.get_string_value("ssl_private_key_password", "", config_group_name);

        ssl_ca_pem_file = config.get_string_value("ssl_ca_pem_file", "", config_group_name);
        ssl_ca_pem_file = FileSystemUtilities::convert_relative_path_to_absolute_path(ssl_ca_pem_file);
        ssl_ca_pem_file = FileSystemUtilities::normalise_path(ssl_ca_pem_file);

        ssl_crl_path  = config.get_string_value("ssl_crl_path", "", config_group_name);
        ssl_crl_path = FileSystemUtilities::convert_relative_path_to_absolute_path(ssl_crl_path);
        ssl_crl_path = FileSystemUtilities::normalise_path(ssl_crl_path);

        ssl_verify_peer = config.get_bool_value("ssl_verify_peer", true, config_group_name);
        #endif

        return true;
    }

    bool validate() const
    {
        ///////////////////////////////////////////////////////////////////////////////////////
        // SENDS
        if (send_try_count < 0)
        {
            validation_error = "send_try_count can't be negative.";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // NIC
        if (nic_ringbuffer_rx_size == 0)
        {
            validation_error = "nic_ringbuffer_rx_size should be greater than zero";
            return false;
        }

        if (nic_ringbuffer_tx_size == 0)
        {
            validation_error = "nic_ringbuffer_tx_size should be greater than zero";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // SOCKETS
        if (socket_rx_size == 0)
        {
            validation_error = "socket_rx_size should be greater than zero";
            return false;
        }

        if (socket_tx_size == 0)
        {
            validation_error = "socket_tx_size should be greater than zero";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // USERSPACE BUFFERS
        if (rx_buffer_capacity == 0)
        {
            validation_error = "rx_buffer_capacity should be greater than zero";
            return false;
        }

        if (tx_encode_buffer_capacity == 0)
        {
            validation_error = "tx_encode_buffer_capacity should be greater than zero";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // POLLING
        if (receive_size <= 0)
        {
            validation_error = "receive_size should be positive";
            return false;
        }
        else if (static_cast<std::size_t>(receive_size) > rx_buffer_capacity)
        {
            validation_error = "receive_size can't be greater than rx_buffer_capacity";
            return false;
        }

        if (spin_count <= 0)
        {
            validation_error = "spin_count should be greater than zero";
            return false;
        }

        if (max_poll_events == 0)
        {
            validation_error = "max_poll_events should be greater than zero";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // THREADS
        if (worker_thread_count < 0)
        {
            validation_error = "worker_thread_count can't be negative";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // OTHERS
        if (accept_port <= 0)
        {
            validation_error = "accept_port should be greater than zero";
            return false;
        }

        if (accept_timeout_seconds <= 0)
        {
            validation_error = "accept_timeout_seconds should be greater than zero";
            return false;
        }

        if (pending_connection_queue_size <= 0)
        {
            validation_error = "pending_connection_queue_size should be greater than zero";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // SSL
        #ifdef LLFIX_ENABLE_OPENSSL
        if (use_ssl)
        {
            if (ssl_version != "TLS10" && ssl_version != "TLS11" && ssl_version != "TLS12" && ssl_version != "TLS13")
            {
                validation_error = "ssl_version should be one of the followings : TLS10, TLS11, TLS12, TLS13";
                return false;
            }

            if (ssl_certificate_pem_file.length() == 0)
            {
                validation_error = "You have to specify ssl_certificate_pem_file config.";
                return false;
            }

            if (FileSystemUtilities::does_file_exist(ssl_certificate_pem_file) == false)
            {
                validation_error = "Specified ssl_certificate_pem_file does not exist";
                return false;
            }

            if (ssl_private_key_pem_file.length() == 0)
            {
                validation_error = "You have to specify ssl_private_key_pem_file config.";
                return false;
            }

            if (FileSystemUtilities::does_file_exist(ssl_private_key_pem_file) == false)
            {
                validation_error = "Specified ssl_private_key_pem_file does not exist";
                return false;
            }

            if (ssl_ca_pem_file.length() == 0)
            {
                validation_error = "You have to specify ssl_ca_pem_file config.";
                return false;
            }

            if (FileSystemUtilities::does_file_exist(ssl_ca_pem_file) == false)
            {
                validation_error = "Specified ssl_ca_pem_file does not exist";
                return false;
            }

            if(!ssl_crl_path.empty())
            {
                if (FileSystemUtilities::does_path_exist(ssl_crl_path) == false)
                {
                    validation_error = "Specified ssl_crl_path does not exist";
                    return false;
                }
            }
        }
        #endif

        return true;
    }

    std::string to_string(const std::string& delimiter = "\n") const
    {
        std::stringstream ret;

        auto inject_category = [&](const std::string& category_name)
            {
                if (delimiter == ",")
                {
                    ret << "-" << category_name << "=" << delimiter;
                }
            };
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // THREADS
        inject_category("THREADS");
        ret << "cpu_core_id=" << cpu_core_id << delimiter;
        ret << "worker_thread_count=" << worker_thread_count << delimiter;

        // HIGH AVAILABILITY
        inject_category("HIGH AVAILABILITY");
        ret << "starts_as_primary_instance=" << std::boolalpha << starts_as_primary_instance << delimiter;
        ret << "refresh_resend_cache_during_promotion=" << std::boolalpha << refresh_resend_cache_during_promotion << delimiter;

        // OTHERS
        inject_category("ACCEPTANCE");
        ret << "accept_port=" << accept_port << delimiter;
        ret << "accept_timeout_seconds=" << accept_timeout_seconds << delimiter;
        ret << "pending_connection_queue_size=" << pending_connection_queue_size << delimiter;

        // NIC
        inject_category("NIC");
        ret << "nic_address=" << nic_address << delimiter;
        ret << "nic_name=" << nic_name << delimiter;
        ret << "nic_ringbuffer_tx_size=" << nic_ringbuffer_tx_size << delimiter;
        ret << "nic_ringbuffer_rx_size=" << nic_ringbuffer_rx_size << delimiter;

        // SENDS
        inject_category("SENDS");
        ret << "send_try_count=" << send_try_count << delimiter;

        // SOCKETS
        inject_category("SOCKETS");
        ret << "socket_rx_size=" << socket_rx_size << delimiter;
        ret << "socket_tx_size=" << socket_tx_size << delimiter;

        // USERSPACE BUFFERS
        inject_category("USERSPACE BUFFERS");
        ret << "rx_buffer_capacity=" << rx_buffer_capacity << delimiter;
        ret << "tx_encode_buffer_capacity=" << tx_encode_buffer_capacity << delimiter;

        // TCP
        inject_category("TCP STACK");
        ret << "disable_nagle=" << std::boolalpha << disable_nagle << delimiter;
        ret << "quick_ack=" << std::boolalpha << quick_ack << delimiter;

        // POLLING
        inject_category("POLLING");
        ret << "receive_size=" << receive_size << delimiter;
        ret << "async_io_timeout_nanoseconds=" << async_io_timeout_nanoseconds << delimiter;
        ret << "spin_count=" << spin_count << delimiter;
        ret << "busy_poll_microseconds=" << busy_poll_microseconds << delimiter;
        ret << "max_poll_events=" << max_poll_events << delimiter;

        // SSL
        #ifdef LLFIX_ENABLE_OPENSSL
        inject_category("SSL");
        ret << "use_ssl=" << std::boolalpha << use_ssl << delimiter;
        ret << "ssl_version=" << ssl_version << delimiter;
        ret << "ssl_cipher_suite=" << ssl_cipher_suite << delimiter;
        ret << "ssl_certificate_pem_file=" << ssl_certificate_pem_file << delimiter;
        ret << "ssl_private_key_pem_file=" << ssl_private_key_pem_file << delimiter;
        ret << "ssl_private_key_password=" << ssl_private_key_password << delimiter;
        ret << "ssl_ca_pem_file=" << ssl_ca_pem_file << delimiter;
        ret << "ssl_crl_path=" << ssl_crl_path << delimiter;
        ret << "ssl_verify_peer=" << std::boolalpha << ssl_verify_peer << delimiter;
        #endif
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        return ret.str();
    }
};

} // namespace