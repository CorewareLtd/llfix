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

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "csv_file.h"

#include <llfix/core/compiler/builtin_functions.h>
#include <llfix/core/compiler/unused.h>

#include <llfix/core/utilities/std_string_utilities.h>

#include <llfix/electronic_trading/common/message_serialiser.h>

#include <llfix/fix_constants.h>
#include <llfix/fix_utilities.h>

struct Entry
{
    uint64_t nanosecs_since_epoch = 0;
    bool success = false;
    std::string message;
};

class FixDeserialiser
{
    public:

        bool set_fields_names_from(const std::string& csv_file_path)
        {
            CSVFile csv_file;

            if (csv_file.load_from(csv_file_path))
            {
                for (auto& row : csv_file)
                {
                    if(row.has_column("TAG") && row.has_column("NAME"))
                    {
                        m_field_names[row["TAG"]] = row["NAME"];
                    }
                    else
                    {
                        add_warning("CSV file " + csv_file_path + " does not have expected columns TAG and name");
                        return false;
                    }
                }

                return true;
            }

            return false;
        }

        void deserialise(char* buffer, std::size_t buffer_size)
        {
            auto entry_handler = [&](uint64_t timestamp, bool success, uint64_t msg_len, const std::string& message, std::size_t message_offset)
                {
                    LLFIX_UNUSED(msg_len);
                    LLFIX_UNUSED(message_offset);
                    Entry current_entry;
                    current_entry.nanosecs_since_epoch = timestamp;
                    current_entry.success = success;
                    if(m_dont_process_flag)
                        current_entry.message = message;
                    else
                        current_entry.message = process_fix_message(message);
                    m_entries.push_back(current_entry);
                };

            uint64_t no_of_messages{ 0 };
            std::size_t read_bytes{ 0 };
            llfix::MessageSerialiser<llfix::FixMessageSequenceNumberExtractor>::deserialise(buffer, buffer_size, no_of_messages, read_bytes, entry_handler);
        }

        void sort()
        {
            std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) { return a.nanosecs_since_epoch < b.nanosecs_since_epoch; });
        }

        std::vector<Entry>* get_entries()
        {
            return &m_entries;
        }

        std::vector<std::string> get_warnings() const { return m_warnings; }

        void set_dont_process_flag(bool b) { m_dont_process_flag = b; }

    private:
        std::vector<Entry> m_entries;
        std::unordered_map<std::string, std::string> m_field_names;
        bool m_dont_process_flag = false;
        std::vector<std::string> m_warnings;

        std::string process_fix_message(const std::string& message)
        {
            std::string ret;
            ret.reserve(message.length());

            auto tag_value_pairs = llfix::StringUtilities::split(message, llfix::FixConstants::FIX_DELIMITER);

            if(tag_value_pairs.size() == 0)
            {
                add_warning("No FIX delimiter in : " + llfix::FixUtilities::fix_to_human_readible(message.c_str(), message.size()));
            }

            for (const auto& tag_value_pair : tag_value_pairs)
            {
                auto tokens = llfix::StringUtilities::split(tag_value_pair, llfix::FixConstants::FIX_EQUALS);

                if(tokens.size() >= 2)
                {
                    auto tag = tokens[0];

                    if (m_field_names.find(tag) != m_field_names.end())
                    {
                        tag = m_field_names[tag];
                    }

                    ret += tag + "=" + tokens[1] + '|';
                }
                else
                {
                    if(tag_value_pair.size()>0)
                    {
                        add_warning("No equals sign in " + tag_value_pair + " full message: " + llfix::FixUtilities::fix_to_human_readible(message.c_str(), message.size()));
                        ret += tag_value_pair + '|';
                    }
                }
            }

            return ret;
        }

        void add_warning(const std::string& warning_message)
        {
            m_warnings.push_back(warning_message);
        }
};