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
#include <type_traits>
#include <string>
#include <string_view>

#include "core/compiler/hints_branch_predictor.h"
#include "core/compiler/unused.h"

#include "core/os/assert_msg.h"

#include "core/utilities/converters.h"
#include "core/utilities/dictionary.h"

#include "electronic_trading/common/fixed_point.h"

#include "fix_constants.h"
#include "fix_string_view.h"
#include "incoming_fix_repeating_groups.h"

namespace llfix
{

struct IncomingValue
{
    FixStringView* value = nullptr;
    uint64_t generation_id = 0;
};

/**
 * @class IncomingFixMessage
 *
 * @brief Represents a parsed incoming FIX message
 *
 * Always call has_tag() or has_repeating_group_tag() before accessing values.
 *
 * Repeating groups are handled separately via IncomingFixRepeatingGroups and
 * accessed through dedicated APIs.
 *
 */
class IncomingFixMessage
{
    public:

        IncomingFixMessage() = default;
        ~IncomingFixMessage() = default;

        bool initialise()
        {
            return m_dict.initialise(64);
        }

        /**
         * @brief Checks whether a FIX tag exists and is valid
         *
         * @param tag FIX tag number.
         * @return true if the tag exists, false otherwise.
         */
        bool has_tag(uint32_t tag) const
        {
            if (m_dict.has_key(tag) == false)
            {
                return false;
            }

            return m_dict[tag].generation_id == m_generation_id;
        }

        void set_tag(uint32_t tag, FixStringView* value)
        {
            IncomingValue node;
            node.value = value;
            node.generation_id = m_generation_id;

            if (llfix_likely(m_dict.has_key(tag)))
            {
                m_dict.set_existing_item(tag, node);
            }
            else
            {
                m_dict.insert(tag, node);
            }
        }

        void copy_non_dirty_tag_values_from(const IncomingFixMessage& other)
        {
            for (const auto& item : other.m_dict)
            {
                if(other.has_tag(item.key))
                {
                    set_tag(item.key, item.value.value);
                }
            }

            m_repeating_groups.copy_non_dirty_values_from(other.m_repeating_groups);
        }

        IncomingFixRepeatingGroups<FixStringView>& get_repeating_groups()
        {
            return m_repeating_groups;
        }

        void set_repeating_group_tag(uint32_t tag, FixStringView* value)
        {
            m_repeating_groups.set(tag, value);
        }

        /**
         * @brief Checks whether a repeating group tag exists.
         *
         * @param tag FIX tag number belonging to a repeating group.
         * @return true if the tag exists in the repeating group storage,
         *         false otherwise.
         */
        bool has_repeating_group_tag(uint32_t tag) const
        {
            return m_repeating_groups.has_tag(tag);
        }

        void reset()
        {
            m_generation_id++;

            if(m_generation_id == 0)
            {
                // Wrap-around protection to avoid stale values
                for (auto& it : m_dict)
                {
                    it.value.generation_id = 0;
                }

                m_generation_id++;
            }

            // Reset the repeating groups
            m_repeating_groups.reset();
        }

        /**
         * @brief Serialises the FIX message into a human-readable string.
         *
         * The output is formatted as:
         * - Header fields first (8, 9, 35, 34, 49, 52, 56)
         * - Body fields (excluding header and trailer)
         * - Repeating group fields
         * - Trailer field (10)
         *
         * Fields are separated by the '|' character instead of SOH.
         *
         * @return A string representation of the FIX message
         *
         */
        std::string to_string() const
        {
            std::string ret;

            auto is_header_tag = [](uint32_t tag)
                {
                    if (tag == FixConstants::TAG_BEGIN_STRING || tag == FixConstants::TAG_BODY_LENGTH || tag == FixConstants::TAG_MSG_TYPE || tag == FixConstants::TAG_MSG_SEQ_NUM || tag == FixConstants::TAG_SENDING_TIME || tag == FixConstants::TAG_SENDER_COMP_ID || tag == FixConstants::TAG_TARGET_COMP_ID)
                        return true;
                    return false;
                };

            auto is_trailer_tag = [](uint32_t tag)
                {
                    if (tag == FixConstants::TAG_CHECKSUM)
                        return true;
                    return false;
                };

            try
            {
                // HEADER
                if(has_tag(FixConstants::TAG_BEGIN_STRING)) ret += "8=" + get_tag_value_as<std::string>(FixConstants::TAG_BEGIN_STRING) + '|';
                if (has_tag(FixConstants::TAG_BODY_LENGTH)) ret += "9=" + get_tag_value_as<std::string>(FixConstants::TAG_BODY_LENGTH) + '|';
                if (has_tag(FixConstants::TAG_MSG_TYPE)) ret += "35=" + get_tag_value_as<std::string>(FixConstants::TAG_MSG_TYPE) + '|';
                if (has_tag(FixConstants::TAG_MSG_SEQ_NUM)) ret += "34=" + get_tag_value_as<std::string>(FixConstants::TAG_MSG_SEQ_NUM) + '|';
                if (has_tag(FixConstants::TAG_SENDER_COMP_ID)) ret += "49=" + get_tag_value_as<std::string>(FixConstants::TAG_SENDER_COMP_ID) + '|';
                if (has_tag(FixConstants::TAG_SENDING_TIME)) ret += "52=" + get_tag_value_as<std::string>(FixConstants::TAG_SENDING_TIME) + '|';
                if (has_tag(FixConstants::TAG_TARGET_COMP_ID)) ret += "56=" + get_tag_value_as<std::string>(FixConstants::TAG_TARGET_COMP_ID) + '|';

                // BODY
                for (const auto& item : m_dict)
                {
                    if (item.value.generation_id == m_generation_id)
                    {
                        if (is_header_tag(item.key) == false && is_trailer_tag(item.key) == false)
                        {
                            ret += std::to_string(item.key) + '=' + item.value.value->to_string() + '|';
                        }
                    }
                }

                // BODY - REPEATING GROUPS
                ret += m_repeating_groups.to_string();

                // TRAILER
                if (has_tag(FixConstants::TAG_CHECKSUM)) ret += "10=" + get_tag_value_as<std::string>(FixConstants::TAG_CHECKSUM) + '|';
            }
            catch (...)
            {
                return "An error occured during IncomingFixMessage::to_string call";
            }

            return ret;
        }
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // GET TAG VALUE METHODS
        FixStringView* get_tag_value(uint32_t tag) const
        {
            llfix_assert_msg(m_dict.has_key(tag) == true, "You should call has_tag first");
            llfix_assert_msg(m_dict[tag].generation_id == m_generation_id, "You should call has_tag first");
            return m_dict[tag].value;
        }

        /**
         * @brief Retrieves a FIX tag value converted to the requested type.
         *
         * @tparam T Target return type.
         * @param tag FIX tag number.
         * @param decimal_points Number of decimal points (required for floating-point and FixedPoint types).
         * @return Tag value converted to type T.
         *
         * @note
         * Supported types:
         * - std::string_view
         * - std::string
         * - char
         * - bool
         * - integral types
         * - floating-point types
         * - FixedPoint
         */
        template<typename T>
        T get_tag_value_as(uint32_t tag, std::size_t decimal_points=0) const
        {
            llfix_assert_msg(m_dict.has_key(tag) == true, "You should call has_tag first");
            llfix_assert_msg(m_dict[tag].generation_id == m_generation_id, "You should call has_tag first");

            if constexpr (std::is_same_v<T, std::string>)
            {
                LLFIX_UNUSED(decimal_points);
                return m_dict[tag].value->to_string();

            }
            else if constexpr (std::is_same_v<T, char>)
            {
                LLFIX_UNUSED(decimal_points);
                return m_dict[tag].value->data()[0];
            }
            else if constexpr (std::is_same_v<T, std::string_view>)
            {
                LLFIX_UNUSED(decimal_points);
                return m_dict[tag].value->to_string_view();
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                LLFIX_UNUSED(decimal_points);
                return (m_dict[tag].value->data()[0] == FixConstants::FIX_BOOLEAN_TRUE) ? true : false;
            }
            else if constexpr (std::is_same_v<T, FixedPoint>)
            {
                llfix_assert_msg(decimal_points > 0, "You have to specify a decimal points value that is greater than zero");
                FixedPoint fp;
                fp.set_decimal_points(static_cast<uint32_t>(decimal_points));
                fp.set_from_chars(m_dict[tag].value->c_str(), m_dict[tag].value->length());
                return fp;
            }
            else if constexpr (std::is_floating_point<T>::value)
            {
                llfix_assert_msg(decimal_points > 0, "You have to specify a decimal points value that is greater than zero");
                return static_cast<T>(Converters::chars_to_double(m_dict[tag].value->c_str(), m_dict[tag].value->length(), decimal_points));
            }
            else if constexpr (std::is_integral<T>::value && std::is_signed<T>::value)
            {
                LLFIX_UNUSED(decimal_points);
                return Converters::chars_to_int<int>(m_dict[tag].value->c_str(), m_dict[tag].value->length());
            }
            else if constexpr (std::is_integral<T>::value && sizeof(T) == sizeof(uint64_t))
            {
                LLFIX_UNUSED(decimal_points);
                return Converters::chars_to_unsigned_int<uint64_t>(m_dict[tag].value->c_str(), m_dict[tag].value->length());
            }
            else if constexpr (std::is_integral<T>::value && sizeof(T) == sizeof(uint32_t))
            {
                LLFIX_UNUSED(decimal_points);
                return Converters::chars_to_unsigned_int<uint32_t>(m_dict[tag].value->c_str(), m_dict[tag].value->length());
            }
            else
            {
                static_assert(always_false_v<T>, "get_tag_value_as unsupported type");
            }
        }

        /**
         * @brief Retrieves a repeating group tag value converted to the requested type.
         *
         * @tparam T Target return type.
         * @param tag FIX tag number.
         * @param index Repeating group index (0-based).
         * @param decimal_points Number of decimal places (required for floating-point and FixedPoint).
         * @return Tag value converted to type T.
         *
         * @note
         * Supported types:
         * - std::string_view
         * - std::string
         * - char
         * - bool
         * - integral types
         * - floating-point types
         * - FixedPoint
         */
        template<typename T>
        T get_repeating_group_tag_value_as(uint32_t tag, std::size_t index, std::size_t decimal_points = 0) const
        {
            FixStringView* str_val = m_repeating_groups.get_value(tag, index);
            llfix_assert_msg(str_val != nullptr, "You should call has_repeating_group_tag first");

            if constexpr (std::is_same_v<T, std::string>)
            {
                LLFIX_UNUSED(decimal_points);
                return str_val->to_string();
            }
            else if constexpr (std::is_same_v<T, char>)
            {
                LLFIX_UNUSED(decimal_points);
                return str_val->data()[0];
            }
            else if constexpr (std::is_same_v<T, std::string_view>)
            {
                LLFIX_UNUSED(decimal_points);
                return str_val->to_string_view();
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                LLFIX_UNUSED(decimal_points);
                return (str_val->data()[0] == FixConstants::FIX_BOOLEAN_TRUE) ? true : false;
            }
            else if constexpr (std::is_same_v<T, FixedPoint>)
            {
                llfix_assert_msg(decimal_points > 0, "You have to specify a decimal points value that is greater than zero");
                FixedPoint fp;
                fp.set_decimal_points(static_cast<uint32_t>(decimal_points));
                fp.set_from_chars(str_val->c_str(), str_val->length());
                return fp;
            }
            else if constexpr (std::is_floating_point<T>::value)
            {
                llfix_assert_msg(decimal_points > 0, "You have to specify a decimal points value that is greater than zero");
                return Converters::chars_to_double(str_val->c_str(), str_val->length(), decimal_points);
            }
            else if constexpr (std::is_integral<T>::value && std::is_signed<T>::value)
            {
                LLFIX_UNUSED(decimal_points);
                return Converters::chars_to_int<int>(str_val->c_str(), str_val->length());
            }
            else if constexpr (std::is_integral<T>::value && sizeof(T) == sizeof(uint64_t))
            {
                LLFIX_UNUSED(decimal_points);
                return Converters::chars_to_unsigned_int<uint64_t>(str_val->c_str(), str_val->length());
            }
            else if constexpr (std::is_integral<T>::value && sizeof(T) == sizeof(uint32_t))
            {
                LLFIX_UNUSED(decimal_points);
                return Converters::chars_to_unsigned_int<uint32_t>(str_val->c_str(), str_val->length());
            }
            else
            {
                static_assert(always_false_v<T>, "get_repeating_group_tag_value_as unsupported type");
            }
        }
        //////////////////////////////////////////////////////////////////////////////
        // VALIDATION METHODS
        bool is_tag_value_numeric(uint32_t tag) const
        {
            llfix_assert_msg(m_dict.has_key(tag) == true, "You should call has_tag first");
            llfix_assert_msg(m_dict[tag].generation_id == m_generation_id, "You should call has_tag first");
            return m_dict[tag].value->is_numeric();
        }

        bool validate_count_tag(uint32_t tag, uint32_t& out_reject_message_code) const
        {
            return m_repeating_groups.validate_count_tag(tag, out_reject_message_code);
        }

        Dictionary<uint32_t, IncomingValue>* get_dictionary()
        {
            return &m_dict;
        }

        uint64_t get_generation_id() const { return m_generation_id; }

    private:
        uint64_t m_generation_id = 1;
        mutable Dictionary<uint32_t, IncomingValue> m_dict;
        IncomingFixRepeatingGroups<FixStringView> m_repeating_groups;

        template <typename>
        static constexpr bool always_false_v = false;

        IncomingFixMessage(const IncomingFixMessage& other) = delete;
        IncomingFixMessage& operator= (const IncomingFixMessage& other) = delete;
        IncomingFixMessage(IncomingFixMessage&& other) = delete;
        IncomingFixMessage& operator=(IncomingFixMessage&& other) = delete;
};

} // namespace