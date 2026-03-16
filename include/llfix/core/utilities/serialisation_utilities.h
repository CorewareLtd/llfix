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
#include <cassert>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "../os/memory_mapped_file.h"

#include "std_string_utilities.h"
#include "filesystem_utilities.h"

namespace llfix
{

class SerialisationUtilities
{
    public:

        template <typename DeserialiserType>
        static bool deserialise_file(DeserialiserType& deserialiser, const std::string& serialised_file_path)
        {
            char* current_buffer = nullptr;
            std::size_t current_buffer_size = 0;

            MemoryMappedFile current_file;

            if (current_file.open(serialised_file_path, 0) == false)
            {
                return false;
            }

            current_file.get_buffer(&current_buffer, current_buffer_size);

            deserialiser.deserialise(current_buffer, current_buffer_size);

            return true;
        }

        static std::vector<std::string> get_all_serialised_files(const std::string& serialisation_path, const std::string& extension="bin")
        {
            std::vector<std::string> ret;

            auto files = FileSystemUtilities::get_all_files_in_a_directory(serialisation_path);

            for (const auto& file_path : files)
            {
                auto bin_file_without_extension = FileSystemUtilities::get_file_from_path_by_excluding_extension(file_path);
                bool is_numeric = !bin_file_without_extension.empty() && std::all_of(bin_file_without_extension.begin(), bin_file_without_extension.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });

                if (is_numeric && StringUtilities::contains(FileSystemUtilities::get_extension_from_file(file_path), extension))
                {
                    ret.push_back(file_path);
                }
            }

            return ret;
        }

        /*
            WHEN MEMORY MAPPED FILES USED FOR SERIALISATION, YOU CAN NOT ADJUST AN EXISTING MEMORY MAPPED FILE'S SIZE
            SINCE IIS TIED TO THE VIRTUAL MEMORY.

            THEREFORE YOU CHOOSE A MAX  MEMORY MAPPED FILE SIZE AND YOU SERIALISE INTO MULTIPLE MEMORY MAPPED FILES IN A DIRECTORY

            SerialisationFolder CLASS IS A HELPER CLASS TO WORK WITH THOSE "SERIALISATION FOLDER"S.

            ASSUMPTIONS :

                1. All serialised files will have the same extension
                2. All serialised file names will be numeric  -> 1 2 3 ...

                An example folder :

                                1.bin
                                2.bin
                                3.bin
                                ...
        */
        class SerialisationFolder
        {
            public:

                bool initialise(const std::string_view& folder, const std::string& extension = "bin")
                {
                    m_folder = folder;

                    bool folder_exists = FileSystemUtilities::does_path_exist(m_folder);

                    if (folder_exists)
                    {
                        auto files = get_all_serialised_files(m_folder, extension);

                        for (const auto& file : files)
                        {
                            m_serialised_file_size = FileSystemUtilities::get_file_size(file);

                            auto current_file_name = FileSystemUtilities::get_file_from_path_by_excluding_extension(file);
                            auto current_file_number = -1;

                            try
                            {
                                current_file_number = std::stoi(current_file_name);
                            }
                            catch (...) {}

                            if (current_file_number != -1)
                            {
                                if (current_file_number < m_earliest_serialised_file_number || m_earliest_serialised_file_number == -1)
                                {
                                    m_earliest_serialised_file_number = current_file_number;
                                }

                                if (current_file_number > m_latest_serialised_file_number)
                                {
                                    m_latest_serialised_file_number = current_file_number;
                                }
                            }
                        }
                    }

                    return folder_exists;
                }

                int get_earliest_serialised_file_number() const { return m_earliest_serialised_file_number; }
                int get_latest_serialised_file_number() const { return m_latest_serialised_file_number; }
                std::string get_path() const { return m_folder; }
                std::size_t serialised_file_size() const { return m_serialised_file_size; }

                std::string get_serialised_file_path(int number, const std::string& extension="bin")
                {
                    assert(number > 0);

                    std::string ret;

                    ret = m_folder + '/' + std::to_string(number) + '.' + extension;

                    return ret;
                }

                bool reset()
                {
                    if (FileSystemUtilities::delete_directory_if_exists(m_folder) == false)
                    {
                        return false;
                    }

                    if (FileSystemUtilities::create_directory(m_folder) == false)
                    {
                        return false;
                    }

                    m_earliest_serialised_file_number = -1;
                    m_latest_serialised_file_number = -1;

                    return true;
                }

            private:
                std::string m_folder;
                int m_earliest_serialised_file_number = -1;
                int m_latest_serialised_file_number = -1;
                std::size_t m_serialised_file_size = 0;
        };
};

} // namespace