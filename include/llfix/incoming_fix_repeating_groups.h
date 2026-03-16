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
#include <cstddef>
#include <string>
#include <vector>

#include "core/compiler/hints_branch_predictor.h"

#include "fix_constants.h"

namespace llfix
{

template<typename FixStringType>
struct IncomingFixRepeatingGroupTagValuePair
{
    uint32_t tag = 0;
    FixStringType* value = nullptr;
};

template<typename FixStringType>
class IncomingFixRepeatingGroups
{
    public:
        IncomingFixRepeatingGroups()
        {
            m_tag_value_pairs.reserve(INITIAL_TAG_VALUE_PAIR_CAPACITY);

            for (std::size_t i = 0; i < INITIAL_TAG_VALUE_PAIR_CAPACITY; i++)
            {
                add_placeholder();
            }
        }

        ~IncomingFixRepeatingGroups() = default;

        void set(uint32_t tag, FixStringType* value)
        {
            if (llfix_unlikely(m_pointer + 1 == m_tag_value_pairs.size()))
            {
                add_placeholder();
            }

            m_tag_value_pairs[m_pointer].tag = tag;
            m_tag_value_pairs[m_pointer].value = value;
            m_pointer++;
        }

        void reset()
        {
            m_pointer = 0;
        }

        void copy_non_dirty_values_from(const IncomingFixRepeatingGroups& other)
        {
            for (std::size_t i = 0; i < other.m_pointer; i++)
            {
                set(other.m_tag_value_pairs[i].tag, other.m_tag_value_pairs[i].value);
            }
            // m_pointer maintained by set
        }

        bool has_any_group() const { return m_pointer > 0; }

        FixStringType* get_value(uint32_t tag, std::size_t index) const
        {
            std::size_t current_index{ 0 };

            for (std::size_t i = 0; i < m_pointer; i++)
            {
                if (m_tag_value_pairs[i].tag == tag)
                {
                    if (current_index == index)
                    {
                        return m_tag_value_pairs[i].value;
                    }
                    current_index++;
                }
            }

            return nullptr;
        }

        bool has_tag(uint32_t tag) const
        {
            for (std::size_t i = 0; i < m_pointer; i++)
            {
                if (m_tag_value_pairs[i].tag == tag)
                {
                    return true;
                }
            }
            return false;
        }

        // Required repeating groups' count/leading tag should appear only once. Not zero times not multiple times
        // They should also have numeric value
        bool validate_count_tag(uint32_t tag, uint32_t& out_reject_message_code) const
        {
            std::size_t appearance{ 0 };

            for (std::size_t i = 0; i < m_pointer; i++)
            {
                if (m_tag_value_pairs[i].tag == tag)
                {
                    if (m_tag_value_pairs[i].value->is_numeric() == false)
                    {
                        out_reject_message_code = FixConstants::FIX_ERROR_CODE_FORMAT_INCORRECT_FOR_TAG;
                        return false;
                    }

                    appearance++;
                }
            }

            if (llfix_likely(appearance == 1))
            {
                return true;
            }

            if (appearance > 1)
            {
                out_reject_message_code = FixConstants::FIX_ERROR_CODE_TAG_APPEARS_MORE_THAN_ONCE;
            }
            else
            {
                out_reject_message_code = FixConstants::FIX_ERROR_CODE_REQUIRED_TAG_MISSING;
            }

            return false;
        }

        std::string to_string() const
        {
            std::string ret;

            for(std::size_t i =0; i <m_pointer; i++)
            {
                ret += std::to_string(m_tag_value_pairs[i].tag) + '=' + m_tag_value_pairs[i].value->to_string() + '|';
            }

            return ret;
        }
        //////////////////////////////////////////////////////////////////////////////
        // Iterator support
        auto begin()
        {
            return m_tag_value_pairs.begin();
        }

        auto end()
        {
            return m_tag_value_pairs.begin() + m_pointer;
        }

    private:
        std::vector<IncomingFixRepeatingGroupTagValuePair<FixStringType>> m_tag_value_pairs;
        std::size_t m_pointer = 0;

        static inline constexpr std::size_t INITIAL_TAG_VALUE_PAIR_CAPACITY = 1024;

        void add_placeholder()
        {
            IncomingFixRepeatingGroupTagValuePair<FixStringType> placeholder;
            m_tag_value_pairs.push_back(placeholder);
        }

        IncomingFixRepeatingGroups(const IncomingFixRepeatingGroups& other) = delete;
        IncomingFixRepeatingGroups& operator= (const IncomingFixRepeatingGroups& other) = delete;
        IncomingFixRepeatingGroups(IncomingFixRepeatingGroups&& other) = delete;
        IncomingFixRepeatingGroups& operator=(IncomingFixRepeatingGroups&& other) = delete;
};

} // namespace