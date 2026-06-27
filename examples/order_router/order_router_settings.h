#pragma once

#include <string>
#include <sstream>

#include <llfix/core/utilities/configuration.h>
#include <llfix/core/utilities/filesystem_utilities.h>

struct OrderRouterSettings
{
    std::string new_order_route_tags;
    std::string replace_order_route_tags;
    std::string cancel_order_route_tags;
    std::string mappings;
    ////////////////////////////////////////////////////////////
    std::string config_load_error;
    mutable std::string validation_error;

    bool load_from_config_file(const std::string& config_file_path, const std::string& config_group_name = "root")
    {
        llfix::Configuration config;

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
                                            "new_order_route_tags", "replace_order_route_tags", "cancel_order_route_tags", "mappings"
                                           }, config_load_error, config_group_name) == false)
        {
            return false;
        }

        new_order_route_tags = config.get_string_value("new_order_route_tags", "", config_group_name);
        replace_order_route_tags = config.get_string_value("replace_order_route_tags", "", config_group_name);
        cancel_order_route_tags = config.get_string_value("cancel_order_route_tags", "", config_group_name);
        mappings = config.get_string_value("mappings", "", config_group_name);

        return true;
    }

    bool validate() const
    {
        if(new_order_route_tags.length()>1)
        {
            if (new_order_route_tags.find(',') == std::string::npos)
            {
                validation_error = "Please provide new_order_route_tags in the following comma separated format : <tag>,<tag>,...";
                return false;
            }
        }

        if(replace_order_route_tags.length()>1)
        {
            if (replace_order_route_tags.find(',') == std::string::npos)
            {
                validation_error = "Please provide replace_order_route_tags in the following comma separated format : <tag>,<tag>,...";
                return false;
            }
        }

        if(cancel_order_route_tags.length()>1)
        {
            if (cancel_order_route_tags.find(',') == std::string::npos)
            {
                validation_error = "Please provide cancel_order_route_tags in the following comma separated format : <tag>,<tag>,...";
                return false;
            }
        }

        if (mappings.length() == 0)
        {
            validation_error = "Please provide mappings in the following comma separated format : <INBOUND_SESSION>-<OUTBOUND_SESSION>,<INBOUND_SESSION>-<OUTBOUND_SESSION>,...";
            return false;
        }

        if(mappings.length()>1)
        {
            if (mappings.find('-') == std::string::npos)
            {
                validation_error = "Please provide mappings in the following comma separated format : <INBOUND_SESSION>-<OUTBOUND_SESSION>,<INBOUND_SESSION>-<OUTBOUND_SESSION>,...";
                return false;
            }
        }

        return true;
    }

    std::string to_string(const std::string& delimiter = "\n") const
    {
        std::stringstream ret;

        ret << "new_order_route_tags=" << new_order_route_tags << delimiter;
        ret << "replace_order_route_tags=" << replace_order_route_tags << delimiter;
        ret << "cancel_order_route_tags=" << cancel_order_route_tags << delimiter;
        ret << "mappings=" << mappings << delimiter;

        return ret.str();
    }
};