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
#include <cassert>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "fix_constants.h"
#include "fix_utilities.h"


namespace llfix
{


class IncomingFixRepeatingGroupSpecs
{
    public:
        IncomingFixRepeatingGroupSpecs() = default;
        ~IncomingFixRepeatingGroupSpecs() = default;

        template<typename CountTag, typename... Rest>
        void specify_repeating_group(const std::string& message_type, CountTag count_tag, Rest... rest)
        {
            static_assert(std::is_integral_v<CountTag>, "All specify_repeating_group arguments must be integral types");
            static_assert((std::is_integral_v<Rest> && ...), "All specify_repeating_group arguments must be integral types");

            add_count_tag(message_type, count_tag);
            (add_repeating_group_tag(message_type, count_tag, rest), ...);
        }

        void add_count_tag(const std::string& message_type, uint32_t tag)
        {
            uint32_t encoded_msg_type = FixUtilities::pack_message_type(std::string_view(message_type));

            if (m_count_tags.find(encoded_msg_type) != m_count_tags.end())
            {
                if (m_count_tags[encoded_msg_type].find(tag) != m_count_tags[encoded_msg_type].end())
                {
                    return;
                }
            }

            m_count_tags[encoded_msg_type][tag] = 1;
        }

        void add_repeating_group_tag(const std::string& message_type, uint32_t count_tag, uint32_t tag)
        {
            uint32_t encoded_msg_type = FixUtilities::pack_message_type(std::string_view(message_type));

            if (m_repeating_group_tags.find(encoded_msg_type) != m_repeating_group_tags.end())
            {
                if (m_repeating_group_tags[encoded_msg_type].find(count_tag) != m_repeating_group_tags[encoded_msg_type].end())
                {
                    if (m_repeating_group_tags[encoded_msg_type][count_tag].find(tag) != m_repeating_group_tags[encoded_msg_type][count_tag].end())
                    {
                        return;
                    }
                }
            }

            m_repeating_group_tags[encoded_msg_type][count_tag][tag] = 1;
        }

        bool is_a_repeating_group_tag(uint32_t encoded_msg_type, uint32_t count_tag, uint32_t tag) const
        {
            auto it_msg = m_repeating_group_tags.find(encoded_msg_type);
            if (it_msg == m_repeating_group_tags.end())
                return false;

            auto it_cnt = it_msg->second.find(count_tag);
            if (it_cnt == it_msg->second.end())
                return false;

            return it_cnt->second.count(tag) != 0;
        }

        bool is_a_repeating_group_count_tag(uint32_t encoded_msg_type, uint32_t tag) const
        {
            auto it_msg = m_count_tags.find(encoded_msg_type);
            if (it_msg == m_count_tags.end())
                return false;

            return it_msg->second.count(tag) != 0;
        }

        std::string to_string(const std::string& message_type="*", uint32_t leading_tag = 0) const
        {
            assert(message_type.size() <= FixConstants::MAX_SUPPORTED_MESSAGE_TYPE_LENGTH);
            std::string ret;

            // COUNT TAGS
            ret += "Count tags :\n";

            for (const auto& item : m_count_tags)
            {
                auto current_message_type = FixUtilities::unpack_message_type(item.first);

                if (current_message_type == message_type || message_type == "*")
                {
                    ret += std::string("\tMessage type '") + current_message_type + "': ";

                    for (const auto& count_tag : m_count_tags[item.first])
                    {
                        if(count_tag.first == leading_tag ||  leading_tag == 0 )
                            ret += std::to_string(count_tag.first) + " ";
                    }

                    ret += "\n";
                }
            }

            ret += "\n";

            // GROUPS
            ret += "Groups :\n";

            for (const auto& item : m_repeating_group_tags)
            {
                auto current_message_type = FixUtilities::unpack_message_type(item.first);

                if (current_message_type == message_type || message_type == "*")
                {
                    ret += std::string("\tMessage type '") + current_message_type + "'\n";

                    for (const auto& nested_item : m_repeating_group_tags[item.first])
                    {
                        if (nested_item.first == leading_tag || leading_tag == 0)
                        {
                            ret += "\t\t" + std::to_string(nested_item.first) + " : ";

                            for (const auto& nested_nested_item : m_repeating_group_tags[item.first][nested_item.first])
                            {
                                ret += std::to_string(nested_nested_item.first) + " ";
                            }

                            ret += "\n";
                        }
                    }
                }
            }

            return ret;
        }

    private:
        mutable std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint8_t>>> m_repeating_group_tags; // The first key is packed message type and the second key is count/leading tag
        mutable std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint8_t>> m_count_tags; // The first key is packed message type and the second key is count/leading tag

        IncomingFixRepeatingGroupSpecs(const IncomingFixRepeatingGroupSpecs& other) = delete;
        IncomingFixRepeatingGroupSpecs& operator= (const IncomingFixRepeatingGroupSpecs& other) = delete;
        IncomingFixRepeatingGroupSpecs(IncomingFixRepeatingGroupSpecs&& other) = delete;
        IncomingFixRepeatingGroupSpecs& operator=(IncomingFixRepeatingGroupSpecs&& other) = delete;
};

} // namespace