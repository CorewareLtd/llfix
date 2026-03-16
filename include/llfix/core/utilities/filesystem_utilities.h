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

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include <fstream>
#include <vector>

#include <filesystem>

namespace llfix
{

class FileSystemUtilities
{
    public:

        static std::vector<std::string> get_all_files_in_a_directory(const std::string& input)
        {
            std::vector<std::string> files;

            try
            {

                for (const auto& entry : std::filesystem::directory_iterator(input))
                {
                    if (std::filesystem::is_regular_file(entry.path()))
                    {
                        files.push_back(std::filesystem::absolute(entry.path()).string());
                    }
                    else if (std::filesystem::is_directory(entry.path()))
                    {
                        auto sub_files = get_all_files_in_a_directory(entry.path().string());
                        files.insert(files.end(), sub_files.begin(), sub_files.end());
                    }
                }
            }
            catch (...) {}

            return files;
        }

        /*
            Unlike std::filesystem implementations , always converting to Linux style
            as Windows can handle both slashes
        */
        static std::string normalise_path(const std::string& input)
        {
            if(input.empty())
            {
                return "";
            }

            std::string ret = input;
            std::replace(ret.begin(), ret.end(), '\\', '/');
            return ret;
        }

        static std::string convert_relative_path_to_absolute_path(const std::string& input)
        {
            if(input.empty())
            {
                return "";
            }

            std::filesystem::path rel_path = input;
            std::filesystem::path abs_path = std::filesystem::absolute(rel_path).lexically_normal();

            return abs_path.string();
        }

        /*
                Input   :   c:/aaa.txt
                Output  :   txt
        */
        static std::string get_extension_from_file(const std::string_view& input)
        {
            std::filesystem::path p{ input };

            if (!p.has_extension())
                return {};

            std::string ext = p.extension().string();

            if (!ext.empty() && ext.front() == '.')
            {
                ext.erase(0, 1);
            }

            return ext;
        }

        /*
                Input   :   c:/aaa.txt
                Output  :   aaa
        */
        static std::string get_file_from_path_by_excluding_extension(const std::string_view& input)
        {
            std::filesystem::path p{ input };
            return p.stem().string();
        }

        /*
            Input   :   c:/aaa.txt
            Output  :   c:/
        */
        static std::string get_path_by_excluding_file(const std::string_view& input)
        {
            std::filesystem::path p(input);
            return p.parent_path().string();
        }

        static bool append_text_to_file(const std::string& file_name, const std::string& text, bool binary_mode = false)
        {
            std::ofstream outfile;

            auto mode = std::ios_base::app | std::ios_base::out;

            if (binary_mode)
            {
                mode = mode | std::ios::binary;
            }

            outfile.open(file_name, mode);

            if (outfile.is_open() == false)
            {
                return false;
            }

            outfile << text;
            outfile.close();
            return true;
        }

        static bool does_file_exist(const std::string& file_name)
        {
            return std::filesystem::exists(file_name);
        }

        static bool does_path_exist(const std::string& path_name)
        {
            std::filesystem::path cur_path(path_name);
            return std::filesystem::exists(cur_path);
        }

        static bool delete_directory_if_exists(const std::string& path)
        {
            if (!std::filesystem::exists(path))
            {
                return false;
            }

            if (!std::filesystem::is_directory(path))
            {
                return false;
            }

            try
            {
                std::filesystem::remove_all(path);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        static bool delete_file_if_exists(const std::string& file_name)
        {
            if (does_file_exist(file_name))
            {
                return std::filesystem::remove(file_name);
            }

            return true;
        }

        static bool create_directory(const std::string& dir_name)
        {
            bool ret{ false };
            try
            {
                ret = std::filesystem::create_directories(dir_name);
            }
            catch (...)
            {
                ret = false;
            }

            return ret;
        }

        static std::size_t get_file_size(const std::string_view& file_path)
        {
            std::filesystem::path path(file_path);
            return std::filesystem::file_size(path);
        }
};

} // namespace