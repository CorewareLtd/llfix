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
    Example config :

        ###############################################
        # USE HASH FOR COMMENTS
        # GROUPS ARE OPTIONAL
        # IF A CONFIG IS NOT UNDER A GROUP IT WILL BE PART OF
        # "ROOT" GROUP
        #
        # GROUP NAMES SHOULD BE UNIQUE
        ###############################################
        [GROUP_NAME]
        LOG_LEVEL=INFO
        FONT_SIZE=8
        ...
*/

#include <string>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <ctype.h>
#include <cstddef>
#include <filesystem>
#include <sstream>

namespace llfix
{

    class Configuration
    {
    public:

        static constexpr inline const char* ROOT_GROUP_NAME = "root";

        bool load_from_file(const std::string& file_name, std::string& error)
        {
            // For reusability
            for (auto& dictionary : m_dictionaries)
            {
                dictionary.second.clear();
            }
            m_dictionaries.clear();

            // Create root group
            add_group(ROOT_GROUP_NAME);

            std::ifstream file(file_name); // ifstream dtor also closes the file so no need for using a smart ptr to close the file

            auto on_error = [&file, &error](const std::string& error_message)
                {
                    error = error_message;
                    file.close();
                };

            if (!file.good())
            {
                on_error("File (" + file_name + ") can`t be opened");
                return false;
            }

            file.seekg(0, std::ios::beg);
            std::string line;
            unsigned long line_number{ 0 };
            std::string current_group_name = ROOT_GROUP_NAME;

            while (std::getline(file, line))
            {
                line_number++;

                trim_in_place(line);
                auto line_length = line.length();

                if(line_length == 0) // Skip empty lines
                {
                    continue;
                }

                if (line.c_str()[0] == '#') // Skip comment lines
                {
                    continue;
                }

                if (line_length < 3)
                {
                    on_error(std::string("Line is too short , line number : ") + std::to_string(line_number));
                    return false;
                }

                if (line.find("[") != std::string::npos && line.find("]") != std::string::npos)
                {
                    current_group_name = line.substr(1, line.size() - 2);

                    if (m_dictionaries.find(current_group_name) != m_dictionaries.end())
                    {
                        on_error(std::string("A group with name ") + current_group_name + " already exists");
                        return false;
                    }
                }
                else
                {
                    std::size_t equals_pos = line.find("=", 0);

                    if (equals_pos == std::string::npos)
                    {
                        on_error(std::string("Line doesn`t contain equals sign , line number : ") + std::to_string(line_number));
                        return false;
                    }

                    auto tokens = split_by_first_separator(line, '=');

                    if (tokens.size() == 2)
                    {
                        std::string attribute = tokens[0];
                        std::string value = tokens[1];
                        add_attribute(attribute, value, current_group_name);
                    }
                }
            }

            file.close();

            return true;
        }

        bool does_attribute_exist(const std::string_view& attribute, const std::string_view& group_name = ROOT_GROUP_NAME) const
        {
            if (does_group_exist(group_name) == false)
            {
                return false;
            }

            auto element = m_dictionaries[group_name.data()].find(attribute.data());
            if (element == m_dictionaries[group_name.data()].end())
            {
                return false;
            }
            return true;
        }

        std::string get_string_value(const std::string_view& attribute, const std::string& default_val_if_absent = "", const std::string_view& group_name = ROOT_GROUP_NAME) const
        {
            if (does_attribute_exist(attribute, group_name) == false)
            {
                return default_val_if_absent;
            }

            auto element = m_dictionaries[group_name.data()].find(attribute.data());
            return element->second;
        }

        bool get_bool_value(const std::string_view& attribute, bool default_val_if_absent = false, const std::string_view& group_name = ROOT_GROUP_NAME) const
        {
            if (does_attribute_exist(attribute, group_name) == false)
            {
                return default_val_if_absent;
            }

            auto string_val = get_string_value(attribute, default_val_if_absent ? "true" : "false", group_name);

            std::transform(string_val.begin(), string_val.end(), string_val.begin(), ::tolower); // To lower
            return (string_val == "true") ? true : false;
        }

        int get_int_value(const std::string_view& attribute, int default_val_if_absent = 0, const std::string_view& group_name = ROOT_GROUP_NAME) const
        {
            int ret{ default_val_if_absent };

            if (does_attribute_exist(attribute, group_name) == false)
            {
                return ret;
            }

            try // std::stoi or std::to_string may throw exception
            {
                ret = std::stoi(get_string_value(attribute, std::to_string(ret), group_name));
            }
            catch (...)
            {
                ret = default_val_if_absent;
            }

            return ret;
        }

        void add_attribute(const std::string_view& attribute, const std::string_view& value, const std::string_view& group_name = ROOT_GROUP_NAME)
        {
            if (does_group_exist(group_name) == false)
            {
                add_group(group_name);
            }

            m_dictionaries[group_name.data()].insert(std::make_pair(attribute, value));
        }

        void save_to_file(const std::string& file_path)
        {
            if (std::filesystem::exists(file_path))
            {
                std::filesystem::remove(file_path);
            }

            std::stringstream content;

            for (const auto& item : m_dictionaries)
            {
                std::string current_group_name = item.first;

                if (current_group_name != ROOT_GROUP_NAME)
                {
                    content << "[" << current_group_name << "]\n";
                }

                for (const auto& iter : item.second)
                {
                    content << iter.first << "=" << iter.second << "\n";
                }
            }

            std::ofstream file(file_path);

            if (file.is_open())
            {
                file << content.str();
                file.close();
            }
        }

        bool does_group_exist(const std::string_view& group_name) const
        {
            auto group_dictionary_entry = m_dictionaries.find(group_name.data());

            if (group_dictionary_entry == m_dictionaries.end())
            {
                return false;
            }

            return true;
        }

        void get_group_names(std::vector<std::string>& target)
        {
            for (const auto& item : m_dictionaries)
            {
                target.push_back(item.first);
            }
        }

        bool validate_loaded_configs(const std::vector<std::string>& expected_configs, std::string& error, const std::string_view& group_name = ROOT_GROUP_NAME) const
        {
            for (const auto& entry : m_dictionaries[group_name.data()])
            {
                auto it = std::find(expected_configs.begin(), expected_configs.end(), entry.first);

                if (it == expected_configs.end())
                {
                    error = "Found unrecognised config -> " + entry.first;
                    return false;
                }
            }
            return true;
        }

    private:

        mutable std::unordered_map<std::string, std::unordered_multimap<std::string, std::string>> m_dictionaries;

        void add_group(const std::string_view& group_name)
        {
            std::unordered_multimap<std::string, std::string> new_group;
            m_dictionaries[group_name.data()] = new_group;
        }

        static void trim_in_place(std::string& input_string)
        {
            trim_left_in_place(input_string);
            trim_right_in_place(input_string);
        }

        static void trim_left_in_place(std::string& input_string)
        {
            // Removes spaces on the left side
            auto iter = std::find_if(input_string.begin(), input_string.end(), [](int ch) { return !std::isspace(ch); });
            input_string.erase(input_string.begin(), iter);
        }

        static void trim_right_in_place(std::string& input_string)
        {
            // Removes spaces on the right side
            auto iter = std::find_if(input_string.rbegin(), input_string.rend(), [](int ch) { return !std::isspace(ch); });
            input_string.erase(iter.base(), input_string.end());
        }

        static std::vector<std::string> split_by_first_separator(const std::string_view& input, char separator)
        {
            std::vector<std::string> ret;
            ret.reserve(2);

            if (input.length() > 0)
            {
                std::size_t first_separator_pos = input.find(separator);
                if (first_separator_pos != std::string::npos)
                {
                    // Found the first separator, split only up to the first occurrence
                    ret.push_back(std::string(input.substr(0, first_separator_pos)));
                    ret.push_back(std::string(input.substr(first_separator_pos + 1)));
                }
                else
                {
                    // No separator found, return the entire input as a single token
                    ret.push_back(input.data());
                }
            }

            return ret;
        }
    };

} // namespace