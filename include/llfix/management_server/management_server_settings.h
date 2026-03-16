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

#include <string>
#include <sstream>
#include "../core/utilities/configuration.h"

namespace llfix
{

struct ManagementServerSettings
{
    std::string management_server_nic_ip;
    int management_server_port = 0;
    int management_server_cpu_core_id = -1;
    ////////////////////////////////////////////////////////////
    mutable std::string validation_error;

    bool load_from_config_file(const std::string& config_file_path, const std::string& config_group_name)
    {
        Configuration config;
        std::string config_load_error;

        if (config.load_from_file(config_file_path, config_load_error) == false)
        {
            return false;
        }

        if (config.does_group_exist(config_group_name) == false)
        {
            config_load_error = config_group_name + " does not exist";
            return false;
        }

        management_server_port = config.get_int_value("management_server_port", 42, config_group_name);
        management_server_nic_ip = config.get_string_value("management_server_nic_ip" , "" , config_group_name);
        management_server_cpu_core_id = config.get_int_value("management_server_cpu_core_id", -1, config_group_name);

        return true;
    }

    bool specified_by_user(const std::string& config_file_path, const std::string& config_group_name) const
    {
        Configuration config;
        std::string config_load_error;

        if (config.load_from_file(config_file_path, config_load_error) == false)
        {
            return false;

        }

        if (config.does_attribute_exist("management_server_port", config_group_name))
        {
            return true;
        }

        return false;
    }

    bool validate() const
    {
        if (management_server_nic_ip.length() == 0)
        {
            validation_error = "management_server_nic_ip length should be greater than zero";
            return false;
        }

        if (management_server_port <= 0)
        {
            validation_error = "management_server_port length should be greater than zero";
            return false;
        }

        return true;
    }

    std::string to_string(const std::string& delimiter = "\n") const
    {
        std::stringstream ret;
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        ret << "management_server_port=" << management_server_port << delimiter;
        ret << "management_server_nic_ip=" << management_server_nic_ip << delimiter;
        ret << "management_server_cpu_core_id=" << management_server_cpu_core_id << delimiter;
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        return ret.str();
    }
};

} // namespace