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
// USED FOR ONLY RX

#include <cstdint>
#include <cassert>
#include <string>
#include <string_view>
#include <unordered_map>

#include "fix_constants.h"
#include "fix_utilities.h"

namespace llfix
{

class IncomingFixBinaryFieldSpecs
{
    public:

        IncomingFixBinaryFieldSpecs() = default;
        ~IncomingFixBinaryFieldSpecs() = default;

        void specify_binary_field(const std::string& message_type, uint32_t tag_length, uint32_t tag_data)
        {
            uint32_t encoded_msg_type = FixUtilities::pack_message_type(std::string_view(message_type));
            m_specifications[encoded_msg_type][tag_length] = tag_data;
        }

        bool is_binary_data_length_tag(uint32_t encoded_msg_type, uint32_t binary_data_length_tag) const
        {
            auto it_msg = m_specifications.find(encoded_msg_type);

            if (it_msg == m_specifications.end())
                return false;

            auto it_cnt = it_msg->second.find(binary_data_length_tag);
            if (it_cnt == it_msg->second.end())
                return false;

            return true;
        }

        std::string to_string(const std::string& message_type = "*", uint32_t length_tag = 0) const
        {
            assert(message_type.size() <= FixConstants::MAX_SUPPORTED_MESSAGE_TYPE_LENGTH);
            std::string ret;

            for (const auto& item : m_specifications)
            {
                auto current_message_type = FixUtilities::unpack_message_type(item.first);

                if (current_message_type == message_type || message_type == "*")
                {
                    ret += std::string("\tMessage type '") + current_message_type + "': ";

                    for (const auto& specification : m_specifications[item.first])
                    {
                        if (specification.first == length_tag || length_tag == 0)
                            ret += std::to_string(specification.first) + " " + std::to_string(specification.second) + " ";
                    }

                    ret += "\n";
                }
            }

            ret += "\n";

            return ret;
        }

    private:
        mutable std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> m_specifications; // The first key is encoded message type and the second key is binary length tag.

        IncomingFixBinaryFieldSpecs(const IncomingFixBinaryFieldSpecs& other) = delete;
        IncomingFixBinaryFieldSpecs& operator= (const IncomingFixBinaryFieldSpecs& other) = delete;
        IncomingFixBinaryFieldSpecs(IncomingFixBinaryFieldSpecs&& other) = delete;
        IncomingFixBinaryFieldSpecs& operator=(IncomingFixBinaryFieldSpecs&& other) = delete;

};

}