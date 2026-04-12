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

#include "common.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>

#include "core/os/vdso.h"
#include "core/os/process_utilities.h"
#include "core/os/thread_local_storage.h"
#include "core/os/hangup_signal_handler.h"
#include "core/os/virtual_memory.h"

#ifdef LLFIX_ENABLE_NUMA // VOLTRON_EXCLUDE
#include "core/os/numa_utilities.h"
#endif // VOLTRON_EXCLUDE

#ifdef LLFIX_ENABLE_TCPDIRECT // VOLTRON_EXCLUDE
#include "core/solarflare_tcpdirect/tcpdirect_api.h"
#endif // VOLTRON_EXCLUDE

#ifdef LLFIX_ENABLE_OPENSSL // VOLTRON_EXCLUDE
#include "core/ssl/ssl_api.h"
#endif // VOLTRON_EXCLUDE

#include "core/os/socket.h"
#include "core/utilities/logger.h"
#include "core/utilities/version.h"
#include "core/utilities/allocator.h"

#include "engine_settings.h"

#include "management_server/management_server_settings.h"
#include "management_server/management_server.h"

namespace llfix
{

/**
 * @class Engine
 * @brief Singleton Engine instance
 *
 * This class is responsible for managing subsystems that are shared between FIX instances : TCP admin interface, logging, allocations, thread local storage, OpenSSL and Solarflare TCPDirect
 */
class Engine
{
    public:

        /**
         * @brief Initialise and start the engine.
         *
         * Should be called before initialising any FIX client/FIX server
         * Note : Not thread safe
         *
         * @param config_file_path Optional path to configuration file.
         * @param config_group_name Optional configuration group name (default "ENGINE").
         */
        static void on_start(const std::string& config_file_path = "", const std::string& config_group_name ="ENGINE")
        {
            if(m_engine_initialised.load() == true)
            {
                return;
            }

            #ifdef __linux__
            if(ProcessUtilities::has_root_privileges() == false)
            {
                fprintf(stderr, "llfix engine : Requires root privileges on Linux.\n");
                std::exit(-1);
            }
            #endif

            m_application_start_timestamp = VDSO::nanoseconds_monotonic();

            EngineSettings engine_settings;

            if(config_file_path.length()>0)
            {
                if(engine_settings.load_from_config_file(config_file_path, config_group_name) == false)
                {
                    fprintf(stderr, "Failed to load %s : %s \n", config_file_path.c_str(), engine_settings.config_load_error.c_str());
                    std::exit(-2);
                }
            }

            if(engine_settings.validate() == false)
            {
                fprintf(stderr, "llfix engine : Failed to verify %s : %s \n", config_file_path.c_str(), engine_settings.validation_error.c_str());
                std::exit(-3);
            }

            HangupSignalHandler::set_ignore(engine_settings.ignore_sighup);

            if( Logger<>::get_instance().initialise(engine_settings.log_file, Logger<>::convert_string_to_log_level(engine_settings.log_level), engine_settings.logger_async, engine_settings.logger_buffer_capacity) == false)
            {
                fprintf(stderr, "llfix engine : Logger initialisation failed\n");
                std::exit(-4);
            }

            if(engine_settings.numa_bind_node >= 0)
            {
                #ifdef _WIN32
                fprintf(stderr, "WARNING : numa_bind_node is specified however NUMA features supported only on Linux.\n");
                #elif __linux__
                #ifdef LLFIX_ENABLE_NUMA
                if(NumaUtilities::get_current_numa_node() == -1)
                {
                    fprintf(stderr, "WARNING : numa_bind_node is set however there is no NUMA node on the system, therefore it won't take affect\n");
                }
                else
                {
                    if( NumaUtilities::bind_to_numa_node(engine_settings.numa_bind_node) == false)
                    {
                        fprintf(stderr, "llfix engine : Failed to bind to NUMA node %d \n", engine_settings.numa_bind_node);
                        std::exit(-5);
                    }

                    LLFIX_LOG_INFO("Bound NUMA node : " + std::to_string(NumaUtilities::get_current_numa_node()));
                    Allocator::set_numa_aware(engine_settings.numa_aware_allocations);
                }
                #else
                fprintf(stderr, "WARNING : numa_bind_node is set however this build not built with LLFIX_ENABLE_NUMA flag, therefore it won't take affect\n");
                #endif
                #endif
            }

            if(engine_settings.lock_pages)
            {
                #ifdef __linux__
                auto page_lock_success = VirtualMemory::lock_all_pages();
                LLFIX_LOG_INFO("VM pages locked : " + std::string( (page_lock_success?"1":"0") ));
                #endif
            }

            if (ThreadLocalStorage::get_instance().create() == false)
            {
                LLFIX_LOG_ERROR("Failed to create thread local storage");
                std::exit(-5);
            }

            Socket<>::socket_library_initialise();

            ManagementServerSettings management_server_settings;

            if (management_server_settings.specified_by_user(config_file_path, config_group_name))
            {
                if (management_server_settings.load_from_config_file(config_file_path, config_group_name) == true)
                {
                    if (management_server_settings.validate() == true)
                    {
                        std::filesystem::path cwd = std::filesystem::current_path();
                        std::filesystem::path full_log_path = cwd / engine_settings.log_file;

                        if (m_management_server.create(management_server_settings, m_application_start_timestamp, m_version.to_string(), full_log_path.string()))
                        {
                            m_management_server_started = true;
                            LLFIX_LOG_INFO("Started management server , port : " + std::to_string(management_server_settings.management_server_port) + " nic : " + management_server_settings.management_server_nic_ip);
                        }
                        else
                        {
                            LLFIX_LOG_ERROR("Failed to create management server");
                        }
                    }
                    else
                    {
                        LLFIX_LOG_ERROR("Failed to validate management server settings. Error : " + management_server_settings.validation_error);
                    }
                }
            }

            LLFIX_LOG_INFO("LLFIX ENGINE Loaded config =>\n" + engine_settings.to_string());

            #ifndef LLFIX_BUILD_CONFIG_GENERATED
            LLFIX_LOG_INFO("LLFIX ENGINE LIBRARY TYPE=HEADER-ONLY , version : " + m_version.to_string());
            #else
            LLFIX_LOG_INFO("LLFIX ENGINE LIBRARY TYPE=STATIC-LIB , version : " + m_version.to_string());
            #endif

            #ifdef LLFIX_ENABLE_TCPDIRECT
            LLFIX_LOG_INFO("LLFIX ENGINE TCPDIRECT=ON , TCPDirect library version : " + std::string(TCPDirectApi::get_version()));
            #else
            LLFIX_LOG_INFO("LLFIX ENGINE TCPDIRECT=OFF");
            #endif

            #ifdef LLFIX_ENABLE_NUMA
            LLFIX_LOG_INFO("LLFIX ENGINE LIBNUMA=ON");
            #else
            LLFIX_LOG_INFO("LLFIX ENGINE LIBNUMA=OFF");
            #endif

            #ifdef LLFIX_ENABLE_OPENSSL
            LLFIX_LOG_INFO("LLFIX ENGINE OPENSSL=ON , OpenSSL library version : " + std::string(SSLApi::get_version()));

            if (SSLApi::get_version_major_number() < 3)
            {
                LLFIX_LOG_ERROR("OpenSSL major version >= 3.0.0 required");
                std::exit(-6);
            }

            if ( !(SSLApi::get_version_major_number() == 3 && SSLApi::get_version_minor_number() == 6) )
            {
                fprintf(stderr, "WARNING : Supported OpenSSL version is 3.6.\n");
            }

            LLFIX_LOG_INFO("LLFIX ENGINE OPENSSL=ON , OpenSSL initialisation : " + std::string(SSLApi::initialise()?"true":"false"));
            #else
            LLFIX_LOG_INFO("LLFIX ENGINE OPENSSL=OFF");
            #endif

            #ifdef LLFIX_ENABLE_DICTIONARY
            LLFIX_LOG_INFO("LLFIX ENGINE DICTIONARY=ON");
            #else
            LLFIX_LOG_INFO("LLFIX ENGINE DICTIONARY=OFF");
            #endif

            #ifdef LLFIX_ENABLE_BINARY_FIELDS
            LLFIX_LOG_INFO("LLFIX BINARY FIELDS SUPPORT=ON");
            #else
            LLFIX_LOG_INFO("LLFIX BINARY FIELDS SUPPORT=OFF");
            #endif

            LLFIX_LOG_INFO("LLFIX ENGINE initialised successfully");

            m_engine_initialised.store(true);
        }

        /**
         * @brief Stops the engine by releasing resources
         *
         * Safe to call multiple times. Ensures the management server is stopped cleanly.
         */
        static void shutdown()
        {
            stop_management_server();

            ThreadLocalStorage::get_instance().destroy();

            #ifdef LLFIX_ENABLE_OPENSSL
            SSLApi::uninitialise();
            #endif

            #ifdef LLFIX_ENABLE_TCPDIRECT
            TCPDirectApi::uninitialise();
            #endif
        }

        /**
         * @brief Stops the management server
         *
         * Safe to call multiple times. Ensures the management server is stopped cleanly.
         */
        static void stop_management_server()
        {
            if(m_management_server_stopped == false)
            {
                if (m_management_server_started == true)
                {
                    LLFIX_LOG_INFO("Stopping management server");
                    m_management_server.stop();
                }
                m_management_server_stopped = true;
            }
        }

        /**
         * @brief Get reference to the management server instance.
         *
         * @return Reference to the singleton ManagementServer instance.
         */
        static ManagementServer& get_management_server()
        {
            return m_management_server;
        }

        static bool initialised() { return m_engine_initialised.load(); }
        static const Version& get_version() { return m_version; }
        static uint64_t get_application_start_timestamp() { return m_application_start_timestamp; }
    private:
        static inline Version m_version{"1.0.1"};
        static inline uint64_t m_application_start_timestamp = 0;
        static inline std::atomic<bool> m_engine_initialised = false;

        static inline ManagementServer m_management_server;
        static inline bool m_management_server_started = false;
        static inline bool m_management_server_stopped = false;
};

} // namespace