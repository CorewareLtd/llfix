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

namespace llfix
{

struct EngineSettings
{
    // NUMA
    int numa_bind_node = -1;
    bool numa_aware_allocations = false;
    // VIRTUAL MEMORY
    bool lock_pages = false;
    // LOGGER
    std::string log_level = "INFO";
    std::string log_file = "log.txt";
    bool logger_async = false;
    std::size_t logger_buffer_capacity = 65536;
    // SIGNALS
    bool ignore_sighup = false;
    ////////////////////////////////////////////////////////////
    std::string config_load_error;
    mutable std::string validation_error;

    bool load_from_config_file(const std::string& config_file_path, const std::string& config_group_name = "ENGINE")
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
                                            // NUMA
                                            "numa_bind_node", "numa_aware_allocations",
                                            // VIRTUAL MEMORY
                                            "lock_pages",
                                            // LOGGER
                                            "log_level", "log_file", "logger_async", "logger_buffer_capacity",
                                            // SIGNALS
                                            "ignore_sighup",
                                            // MANAGEMENT SERVER
                                            "management_server_port", "management_server_nic_ip", "management_server_cpu_core_id"
                                           }, config_load_error, config_group_name) == false)
        {
            return false;
        }

        // NUMA
        numa_bind_node = config.get_int_value("numa_bind_node", -1, config_group_name);
        numa_aware_allocations = config.get_bool_value("numa_aware_allocations", false, config_group_name);

        // VIRTUAL MEMORY
        lock_pages = config.get_bool_value("lock_pages", false, config_group_name);

        // LOGGER
        log_level = config.get_string_value("log_level", "INFO", config_group_name);
        log_file = config.get_string_value("log_file", "log.txt", config_group_name);
        logger_async = config.get_bool_value("logger_async", false, config_group_name);
        logger_buffer_capacity = static_cast<std::size_t>(config.get_int_value("logger_buffer_capacity", 65536, config_group_name));

        // SIGNALS
        ignore_sighup = config.get_bool_value("ignore_sighup", false, config_group_name);

        return true;
    }

    bool validate() const
    {
        // LOGGER
        if (log_level.length() == 0)
        {
            validation_error = "log_level length should be greater than zero";
            return false;
        }
        else
        {
            if( log_level != "INFO" && log_level != "DEBUG" && log_level != "ERROR" && log_level != "WARNING" && log_level != "FATAL")
            {
                validation_error = "log_level should be one of : INFO,DEBUG,ERROR,WARNING,FATAL";
                return false;
            }
        }

        if (log_file.length() == 0)
        {
            validation_error = "log_file length should be greater than zero";
            return false;
        }

        if(logger_buffer_capacity<1024)
        {
            validation_error = "logger_buffer_capacity is too low";
            return false;
        }

        return true;
    }

    std::string to_string(const std::string& delimiter = "\n") const
    {
        std::stringstream ret;
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // NUMA
        ret << "numa_bind_node=" << numa_bind_node << delimiter;
        ret << "numa_aware_allocations=" << std::boolalpha << numa_aware_allocations << delimiter;
        // VIRTUAL MEMORY
        ret << "lock_pages=" << std::boolalpha << lock_pages << delimiter;
        // LOGGER
        ret << "log_level=" << log_level << delimiter;
        ret << "log_file=" << log_file << delimiter;
        ret << "logger_async=" << std::boolalpha << logger_async << delimiter;
        ret << "logger_buffer_capacity=" << logger_buffer_capacity << delimiter;
        // SIGNALS
        ret << "ignore_sighup=" << std::boolalpha << ignore_sighup << delimiter;
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        return ret.str();
    }
};

} // namespace