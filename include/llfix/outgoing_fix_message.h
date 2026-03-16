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
    - Encoded message layout :  <header=t8,t9,t35,t34,t49,t52,t56> + <optional header tag369> + <optional static header tags> + <optional header tags> + <body> + <optional trailer tags> + <checksum/t10>
*/
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/compiler/builtin_functions.h"
#include "core/compiler/unused.h"
#include "core/compiler/hints_branch_predictor.h"
#include "core/compiler/hints_hot_code.h"

#include "core/os/assert_msg.h"

#include "core/utilities/converters.h"
#include "core/utilities/object_cache.h"

#include "electronic_trading/common/fixed_point.h"
#include "electronic_trading/session/sequence_store.h"

#include "fix_constants.h"
#include "fix_string.h"
#include "fix_session_settings.h"
#include "fix_utilities.h"

namespace llfix
{

/**
 * @enum FixMessageComponent
 * @brief Identifies the logical component of a FIX message.
 *
 * Specifies where a FIX tag should be placed within an outgoing
 * FIX message. This enum is primarily used as a template parameter
 * to control tag routing when building messages with
 * OutgoingFixMessage.
 *
 * ### FIX Message Structure
 * - HEADER  : Session-level and administrative fields
 * - BODY    : Application-level message fields
 * - TRAILER : Trailer fields appended before checksum calculation
 *
 */
enum class FixMessageComponent
{
    HEADER,
    BODY,
    TRAILER
};

struct OutgoingValue
{
    uint32_t tag = 0;
    FixString* value = nullptr;
    char tag_str[16];
    std::size_t tag_str_length=0;
};

struct OutgoingStaticValue
{
    uint32_t tag = 0;
    char tag_str[16];
    std::size_t tag_str_length=0;
    std::string value;
};

/**
 * @class OutgoingFixMessage
 * @brief FIX message builder and encoder for outbound messages.
 *
 */
class OutgoingFixMessage
{
    public:

        OutgoingFixMessage() = default;
        ~OutgoingFixMessage() = default;

        bool initialise(FixSessionSettings* session_settings_instance, SequenceStore* session_sequence_store_instance)
        {
            m_session_settings = session_settings_instance;
            m_session_sequence_store = session_sequence_store_instance;

            m_body_vector.reserve(INITIAL_BODY_TAG_PLACEHOLDER_COUNT);

            for (std::size_t i = 0; i < INITIAL_BODY_TAG_PLACEHOLDER_COUNT; i++)
            {
                add_placeholder_to_body_vector();
            }

            m_header_vector.reserve(INITIAL_BODY_TAG_PLACEHOLDER_COUNT);

            for (std::size_t i = 0; i < INITIAL_BODY_TAG_PLACEHOLDER_COUNT; i++)
            {
                add_placeholder_to_header_vector();
            }

            m_trailer_vector.reserve(INITIAL_BODY_TAG_PLACEHOLDER_COUNT);

            for (std::size_t i = 0; i < INITIAL_BODY_TAG_PLACEHOLDER_COUNT; i++)
            {
                add_placeholder_to_trailer_vector();
            }

            return m_fix_string_cache.create(256);
        }

        /**
         * @brief Sets FIX MsgType (tag 35) using a single character.
         *
         * This overload is intended for FIX message types represented by
         * a single character (e.g. 'D', '8', '0').
         *
         * @param c FIX MsgType character
         */
        void set_msg_type(char c)
        {
            m_msg_type[0] = c;
            m_msg_type_len = 1;
        }

        /**
         * @brief Sets FIX MsgType (tag 35) using a string view.
         *
         * Supports multi-character FIX message types
         * (e.g. "AE", "XLR").
         *
         * @param buffer String view containing the message type
         *
         * @pre Maximum buffer length is 4 characters
         */
        void set_msg_type(std::string_view buffer)
        {
            m_msg_type_len = buffer.length();
            assert(m_msg_type_len <= FixConstants::MAX_SUPPORTED_MESSAGE_TYPE_LENGTH);
            llfix_builtin_memcpy(m_msg_type, buffer.data(), m_msg_type_len);
        }

        /**
         * @brief Appends a FIX tag to the specified message component.
         *
         * Encodes the supplied value into an internal FixString and appends
         * it to the selected FIX message component (HEADER, BODY, or TRAILER).
         *
         * ### Supported Value Types
         * - const char*
         * - std::string
         * - std::string_view
         * - char
         * - bool (Y/N)
         * - FixedPoint
         * - Integral types (signed / unsigned)
         * - Floating-point types (requires decimal_points)
         *
         * @tparam component FIX message component to append to
         *         (default: FixMessageComponent::BODY)
         * @tparam T Value type to encode
         *
         * @param tag FIX tag number
         * @param val Value to encode
         * @param decimal_points Number of decimal places for floating-point values
         *
         */
        template<FixMessageComponent component = FixMessageComponent::BODY, typename T>
        void set_tag(uint32_t tag, T val, std::size_t decimal_points = 0)
        {
            FixString* str_value = m_fix_string_cache.allocate();

            if constexpr (std::is_same_v<T, const char*>)
            {
                LLFIX_UNUSED(decimal_points);
                str_value->copy_from(val);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                LLFIX_UNUSED(decimal_points);
                str_value->copy_from(val.c_str());
            }
            else if constexpr (std::is_same_v<T, char>)
            {
                LLFIX_UNUSED(decimal_points);
                str_value->copy_from(val);
            }
            else if constexpr (std::is_same_v<T, std::string_view>)
            {
                LLFIX_UNUSED(decimal_points);
                str_value->copy_from(val);
            }
            else if constexpr (std::is_same_v<T, FixString*>)
            {
                LLFIX_UNUSED(decimal_points);
                str_value->copy_from(val->to_string_view());
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                LLFIX_UNUSED(decimal_points);
                str_value->data()[0] = val == true ? FixConstants::FIX_BOOLEAN_TRUE : FixConstants::FIX_BOOLEAN_FALSE;
                str_value->set_length(1);
            }
            else if constexpr (std::is_same_v<T, FixedPoint>)
            {
                LLFIX_UNUSED(decimal_points);
                auto len = val.to_chars(str_value->data());
                str_value->set_length(static_cast<uint32_t>(len));
            }
            else if constexpr (std::is_floating_point<T>::value)
            {
                llfix_assert_msg(decimal_points > 0, "When you pass double/float to set_tag, you should also specify decimal points");
                auto length = Converters::double_to_chars(val, str_value->data(), str_value->capacity(), decimal_points);
                str_value->set_length(static_cast<uint32_t>(length));
            }
            else if constexpr (std::is_integral<T>::value && std::is_signed<T>::value)
            {
                LLFIX_UNUSED(decimal_points);
                auto length = Converters::int_to_chars(val, str_value->data());
                str_value->set_length(static_cast<uint32_t>(length));
            }
            else if constexpr (std::is_integral<T>::value && sizeof(T) == sizeof(uint64_t))
            {
                LLFIX_UNUSED(decimal_points);
                auto length = Converters::unsigned_int_to_chars<uint64_t>(val, str_value->data());
                str_value->set_length(static_cast<uint32_t>(length));
            }
            else if constexpr (std::is_integral<T>::value && sizeof(T) == sizeof(uint32_t))
            {
                LLFIX_UNUSED(decimal_points);
                auto length = Converters::unsigned_int_to_chars<uint32_t>(val, str_value->data());
                str_value->set_length(static_cast<uint32_t>(length));
            }
            else
            {
                static_assert(always_false_v<T>, "set_tag unsupported type");
            }

            set_tag_internal<component>(tag, str_value);
        }

        /**
         * @brief Appends a FIX tag using the provided buffer with raw data
         *
         * @tparam component FIX message component to append to
         *         (default: FixMessageComponent::BODY)
         *
         * @param tag FIX tag number representing a binary field
         * @param buffer start address of buffer that hold raw/binary data
         * @param data_length length of buffer
         *
         */
        template<FixMessageComponent component = FixMessageComponent::BODY>
        void set_binary_tag(uint32_t tag, const char* buffer, std::size_t data_length)
        {
            FixString* str_value = m_fix_string_cache.allocate();
            str_value->copy_from(buffer, data_length);
            set_tag_internal<component>(tag, str_value);
        }

        /**
         * @brief Appends a FIX timestamp tag using the current session time.
         *
         * Encodes the current timestamp once per message using the session’s
         * configured sub-second precision and reuses it for all timestamp
         * tags within the same message.
         *
         * @tparam component FIX message component to append to
         *         (default: FixMessageComponent::BODY)
         *
         * @param tag FIX tag number representing a timestamp field
         *
         */
        template<FixMessageComponent component = FixMessageComponent::BODY>
        void set_timestamp_tag(uint32_t tag)
        {
            if (m_fix_string_send_time_set == false)
            {
                FixUtilities::encode_current_time(&m_fix_string_send_time, m_session_settings->timestamp_subseconds_precision);
                m_fix_string_send_time_set = true;
            }

            set_tag_internal<component>(tag, &m_fix_string_send_time);
        }

        void set_additional_static_header_tag(uint32_t tag, const std::string& val)
        {
            OutgoingStaticValue node;
            node.tag = tag;
            node.value = val;

            node.tag_str_length = Converters::unsigned_int_to_chars<uint32_t>(node.tag, &(node.tag_str[0]));

            m_additional_static_header_tags.push_back(node);
        }

        /**
         * @brief Returns the currently cached tag 52 (SendingTime) value.
         *
         * @return SendingTime as a string.
         */
        std::string get_sending_time()
        {
            if(m_fix_string_send_time_set == false)
            {
                FixUtilities::encode_current_time(&m_fix_string_send_time, m_session_settings->timestamp_subseconds_precision);
                m_fix_string_send_time_set = true;
            }

            return m_fix_string_send_time.to_string();
        }

        LLFIX_HOT void encode(char* target_buffer, std::size_t target_buffer_length, const uint32_t sequence_no, std::size_t& encoded_length)
        {
            assert(target_buffer != nullptr && target_buffer_length > 0);

            ////////////////////////////////////////////////////////////////
            // 1. t34 / SEQUENCE NO
            auto fix_str_seq_no_length = Converters::unsigned_int_to_chars<uint32_t>(sequence_no, m_fix_string_seq_no.data());
            m_fix_string_seq_no.set_length(static_cast<uint32_t>(fix_str_seq_no_length));

            ////////////////////////////////////////////////////////////////
            // 2. BODY LENGTH

            /*
                Excluding t8 and t9

                For each of 5 (35 34 49 52 56) -> 2 (Tag) 1(equal sign) 1 (delimiter) -> 4*5 -> 20

                Total = 20
            */
            int body_length = 20;

            if (m_fix_string_send_time_set == false)
            {
                FixUtilities::encode_current_time(&m_fix_string_send_time, m_session_settings->timestamp_subseconds_precision);
                m_fix_string_send_time_set = true;
            }

            body_length += static_cast<int>(m_fix_string_send_time.length());

            body_length += static_cast<int>(m_msg_type_len);
            body_length += static_cast<int>(m_fix_string_seq_no.length());
            body_length += static_cast<int>(m_session_settings->sender_comp_id.length());
            body_length += static_cast<int>(m_session_settings->target_comp_id.length());

            if(m_session_settings->include_last_processed_seqnum_in_header)
            {
                auto last_processed_seq_num_str_len = Converters::unsigned_int_to_chars<uint32_t>(m_session_sequence_store->get_incoming_seq_no(), m_last_processed_seq_num.data());
                m_last_processed_seq_num.set_length(static_cast<uint32_t>(last_processed_seq_num_str_len));
                body_length += static_cast<int>(5+ last_processed_seq_num_str_len); // 5 -> 369= and delimiter
            }

            LLFIX_ALIGN_CODE_32;
            for(std::size_t i =0; i < m_header_pointer; i++)
            {
                m_header_vector[i].tag_str_length = Converters::unsigned_int_to_chars<uint32_t>(m_header_vector[i].tag, &(m_header_vector[i].tag_str[0]));
                body_length += static_cast<int>((2 + m_header_vector[i].tag_str_length + m_header_vector[i].value->length())); // 2 is delimiter and equals sign
            }

            LLFIX_ALIGN_CODE_32;
            for(std::size_t i =0; i < m_body_pointer; i++)
            {
                m_body_vector[i].tag_str_length = Converters::unsigned_int_to_chars<uint32_t>(m_body_vector[i].tag, &(m_body_vector[i].tag_str[0]));
                body_length += static_cast<int>((2 + m_body_vector[i].tag_str_length + m_body_vector[i].value->length())); // 2 is delimiter and equals sign
            }

            LLFIX_ALIGN_CODE_32;
            for(std::size_t i =0; i < m_trailer_pointer; i++)
            {
                m_trailer_vector[i].tag_str_length = Converters::unsigned_int_to_chars<uint32_t>(m_trailer_vector[i].tag, &(m_trailer_vector[i].tag_str[0]));
                body_length += static_cast<int>((2 + m_trailer_vector[i].tag_str_length + m_trailer_vector[i].value->length())); // 2 is delimiter and equals sign
            }

            LLFIX_ALIGN_CODE_32;
            for(const auto & additional_header_tag : m_additional_static_header_tags)
            {
                body_length += static_cast<int>((2 + additional_header_tag.tag_str_length + additional_header_tag.value.length())); // 2 is delimiter and equals sign
            }

            auto fix_str_body_len_length = Converters::unsigned_int_to_chars<uint32_t>(body_length, m_fix_string_body_length.data());
            m_fix_string_body_length.set_length(static_cast<uint32_t>(fix_str_body_len_length));
            //////////////////////////////////////////////////////////////////////////////////////////
            // 3. WRITE ALL INTO THE TARGET BUFFER
            encoded_length = 0;

            auto write_to_buffer = [&target_buffer, &encoded_length](const char* buffer, std::size_t buffer_len)
            {
                assert(buffer_len);
                llfix_builtin_memcpy(target_buffer + encoded_length, buffer, buffer_len);
                encoded_length += buffer_len;
            };

            // t8
            write_to_buffer("8=", 2);
            write_to_buffer(m_session_settings->begin_string.c_str(), m_session_settings->begin_string.length());
            write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);

            // t9
            write_to_buffer("9=", 2);
            write_to_buffer(m_fix_string_body_length.c_str(), m_fix_string_body_length.length());
            write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);

            // t35
            write_to_buffer("35=", 3);
            write_to_buffer(&m_msg_type[0], m_msg_type_len);
            write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);

            // t34
            write_to_buffer("34=", 3);
            write_to_buffer(m_fix_string_seq_no.c_str(), m_fix_string_seq_no.length());
            write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);

            // t49
            write_to_buffer("49=", 3);
            write_to_buffer(m_session_settings->sender_comp_id.c_str(), m_session_settings->sender_comp_id.length());
            write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);

            // t52
            write_to_buffer("52=", 3);
            write_to_buffer(m_fix_string_send_time.data(), m_fix_string_send_time.length());
            write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);

            // t56
            write_to_buffer("56=", 3);
            write_to_buffer(m_session_settings->target_comp_id.c_str(), m_session_settings->target_comp_id.length());
            write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);

            if(m_session_settings->include_last_processed_seqnum_in_header)
            {
                // t369
                write_to_buffer("369=", 4);
                write_to_buffer(m_last_processed_seq_num.c_str(), m_last_processed_seq_num.length());
                write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);
            }

            // ADDITIONAL STATIC HEADER TAGS
            LLFIX_ALIGN_CODE_32;
            for(const auto & additional_header_tag : m_additional_static_header_tags)
            {
                write_to_buffer(&additional_header_tag.tag_str[0], additional_header_tag.tag_str_length);

                write_to_buffer(&FixConstants::FIX_EQUALS, 1);

                write_to_buffer(additional_header_tag.value.c_str(), additional_header_tag.value.length());

                write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);
            }

            // HEADER TAGS
            LLFIX_ALIGN_CODE_32;
            for(std::size_t i=0; i<m_header_pointer;i++)
            {
                write_to_buffer(&m_header_vector[i].tag_str[0], m_header_vector[i].tag_str_length);

                write_to_buffer(&FixConstants::FIX_EQUALS, 1);

                write_to_buffer(m_header_vector[i].value->c_str(), m_header_vector[i].value->length());

                write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);
            }

            // BODY TAGS
            LLFIX_ALIGN_CODE_32;
            for(std::size_t i=0; i<m_body_pointer;i++)
            {
                write_to_buffer(&m_body_vector[i].tag_str[0], m_body_vector[i].tag_str_length);

                write_to_buffer(&FixConstants::FIX_EQUALS, 1);

                write_to_buffer(m_body_vector[i].value->c_str(), m_body_vector[i].value->length());

                write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);
            }

            // TRAILER TAGS
            LLFIX_ALIGN_CODE_32;
            for(std::size_t i=0; i<m_trailer_pointer;i++)
            {
                write_to_buffer(&m_trailer_vector[i].tag_str[0], m_trailer_vector[i].tag_str_length);

                write_to_buffer(&FixConstants::FIX_EQUALS, 1);

                write_to_buffer(m_trailer_vector[i].value->c_str(), m_trailer_vector[i].value->length());

                write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);
            }

            //////////////////////////////////////////////////////////////////////////////////////////
            // 4. CALCULATE CHECKSUM AND WRITE IT AS T10
            write_to_buffer("10=", 3);

            if(llfix_likely(m_session_settings->enable_simd_avx2))
            {
                FixUtilities::encode_checksum_simd_avx2(target_buffer, encoded_length-3, target_buffer + encoded_length);
            }
            else
            {
                FixUtilities::encode_checksum_no_simd(target_buffer, encoded_length-3, target_buffer + encoded_length);
            }
            encoded_length += FixConstants::FIX_CHECKSUM_LENGTH;

            write_to_buffer(&(FixConstants::FIX_DELIMITER), 1);

            //////////////////////////////////////////////////////////////////////////////////////////
            // 5. PREPARE FOR NEXT ENCODE CALL
            m_header_pointer = 0;
            m_body_pointer = 0;
            m_trailer_pointer = 0;
            m_fix_string_cache.reset_pointer();
            m_fix_string_send_time_set = false;
        }

        std::string to_string()
        {
            std::string ret;

            for(std::size_t i=0; i< m_header_pointer; i++)
            {
                try
                {
                    ret += std::to_string(m_header_vector[i].tag) + "=" + m_header_vector[i].value->to_string() + '|';
                }
                catch (...)
                {
                    return "An error occurred during OutgoingFixMessage::to_string call";
                }
            }

            for(std::size_t i=0; i< m_body_pointer; i++)
            {
                try
                {
                    ret += std::to_string(m_body_vector[i].tag) + "=" + m_body_vector[i].value->to_string() + '|';
                }
                catch (...)
                {
                    return "An error occurred during OutgoingFixMessage::to_string call";
                }
            }

            for(std::size_t i=0; i< m_trailer_pointer; i++)
            {
                try
                {
                    ret += std::to_string(m_trailer_vector[i].tag) + "=" + m_trailer_vector[i].value->to_string() + '|';
                }
                catch (...)
                {
                    return "An error occurred during OutgoingFixMessage::to_string call";
                }
            }

            return ret;
        }

        // Used for resending previously sent messages while responding to an incoming 35=2
        void load_from_buffer(char* buffer, std::size_t buffer_size)
        {
            assert(buffer);
            assert(buffer_size > 0);

            std::size_t buffer_read{ 0 };
            bool looking_for_equals{ true };

            std::size_t current_tag_start{ 0 };
            std::size_t current_tag_length{ 0 };

            std::size_t current_value_start{ 0 };
            std::size_t current_value_length{ 0 };

            bool is_replacing_a_non_retransmittable_admin_message {false};

            while (true)
            {
                if (looking_for_equals)
                {
                    if (buffer[buffer_read] == FixConstants::FIX_EQUALS)
                    {
                        looking_for_equals = false;

                        current_value_start = buffer_read + 1;
                        current_value_length = 0;
                    }
                    else
                    {
                        current_tag_length++;
                    }
                }
                else // looking for delimiter
                {
                    if (buffer[buffer_read] == FixConstants::FIX_DELIMITER)
                    {
                        uint32_t tag = Converters::chars_to_unsigned_int<uint32_t>(buffer + current_tag_start, current_tag_length);

                        if (tag != FixConstants::TAG_BEGIN_STRING && tag != FixConstants::TAG_BODY_LENGTH && tag != FixConstants::TAG_SENDER_COMP_ID && tag != FixConstants::TAG_TARGET_COMP_ID && tag != FixConstants::TAG_CHECKSUM)
                        {
                            bool is_one_of_static_header_tags = is_static_header_tag(tag);

                            if (is_one_of_static_header_tags == false)
                            {
                                auto str_val = m_fix_string_cache.allocate();
                                str_val->copy_from(buffer + current_value_start, current_value_length);

                                if (tag == FixConstants::TAG_MSG_TYPE) // We need to preserve original t35/msgtype
                                {
                                    is_replacing_a_non_retransmittable_admin_message = FixUtilities::is_a_non_retransmittable_admin_message_type(str_val);

                                    if(is_replacing_a_non_retransmittable_admin_message)
                                    {
                                        set_msg_type(FixConstants::MSG_TYPE_SEQUENCE_RESET);
                                    }
                                    else
                                    {
                                        if (str_val->length() == 1)
                                        {
                                            set_msg_type(str_val->data()[0]);
                                        }
                                        else
                                        {
                                            set_msg_type(str_val->data());
                                        }
                                    }
                                }
                                else if (tag == FixConstants::TAG_SENDING_TIME) // We want to record orig send time in t122/orig sending time
                                {
                                    set_tag<FixMessageComponent::HEADER>(FixConstants::TAG_ORIG_SENDING_TIME, str_val);
                                }
                                else if(tag == FixConstants::TAG_MSG_SEQ_NUM)
                                {
                                    if(is_replacing_a_non_retransmittable_admin_message)
                                    {
                                        uint32_t sequence_no = Converters::chars_to_unsigned_int<uint32_t>(buffer + current_value_start, current_value_length);
                                        sequence_no++;
                                        set_tag(FixConstants::TAG_NEW_SEQ_NO, sequence_no);
                                    }
                                    // Else case of seq num is handled by FixClient/FixServer::resend_messages_to_server
                                }
                                else // Anything else preserved in the same order
                                {
                                    if(is_replacing_a_non_retransmittable_admin_message == false)
                                    {
                                        set_tag(tag, str_val);
                                    }
                                }
                            }
                        }

                        looking_for_equals = true;

                        current_tag_start = buffer_read + 1;
                        current_tag_length = 0;
                    }
                    else
                    {
                        current_value_length++;
                    }
                }

                buffer_read++;

                if (buffer_read == buffer_size)
                {
                    break;
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////////
        // Iterator support, only for body tags
        auto begin()
        {
            return m_body_vector.begin();
        }

        auto end()
        {
            return m_body_vector.begin() + m_body_pointer;
        }
        //////////////////////////////////////////////////////////////////////////////

        #ifdef LLFIX_UNIT_TEST
        std::size_t get_msg_type_length() const { return m_msg_type_len; }
        #endif

    private:
        // HEADER
        char m_msg_type[FixConstants::MAX_SUPPORTED_MESSAGE_TYPE_LENGTH];
        std::size_t m_msg_type_len = 1;

        FixString m_fix_string_send_time;
        bool m_fix_string_send_time_set = false;
        FixString m_fix_string_seq_no;
        FixString m_fix_string_body_length;
        FixString m_last_processed_seq_num;

        std::vector<OutgoingStaticValue> m_additional_static_header_tags;
        std::vector<OutgoingValue> m_header_vector;
        std::size_t m_header_pointer = 0;

        void add_placeholder_to_header_vector()
        {
            OutgoingValue placeholder;
            m_header_vector.push_back(placeholder);
        }

        // BODY
        std::vector<OutgoingValue> m_body_vector;
        std::size_t m_body_pointer = 0;
        static inline constexpr std::size_t INITIAL_BODY_TAG_PLACEHOLDER_COUNT = 256;

        void add_placeholder_to_body_vector()
        {
            OutgoingValue placeholder;
            m_body_vector.push_back(placeholder);
        }

        // TRAILER
        std::vector<OutgoingValue> m_trailer_vector;
        std::size_t m_trailer_pointer = 0;

        void add_placeholder_to_trailer_vector()
        {
            OutgoingValue placeholder;
            m_trailer_vector.push_back(placeholder);
        }

        // Fix string cache
        ObjectCache<FixString> m_fix_string_cache;

        // Session specific instances
        SequenceStore* m_session_sequence_store = nullptr;
        FixSessionSettings* m_session_settings = nullptr;

        bool is_static_header_tag(uint32_t tag) const
        {
            for (const auto& item : m_additional_static_header_tags)
            {
                if (item.tag == tag)
                {
                    return true;
                }
            }
            return false;
        }

        template<FixMessageComponent component = FixMessageComponent::BODY>
        void set_tag_internal(uint32_t tag, FixString* value)
        {
            OutgoingValue node;
            node.tag = tag;
            node.value = value;

            if constexpr (component == FixMessageComponent::BODY)
            {
                if (llfix_unlikely(m_body_pointer + 1 == m_body_vector.size()))
                {
                    add_placeholder_to_body_vector();
                }

                m_body_vector[m_body_pointer] = node;
                m_body_pointer++;
            }
            else if constexpr (component == FixMessageComponent::HEADER)
            {
                if (llfix_unlikely(m_header_pointer + 1 == m_header_vector.size()))
                {
                    add_placeholder_to_header_vector();
                }

                m_header_vector[m_header_pointer] = node;
                m_header_pointer++;
            }
            else if constexpr (component == FixMessageComponent::TRAILER)
            {
                if (llfix_unlikely(m_trailer_pointer + 1 == m_trailer_vector.size()))
                {
                    add_placeholder_to_trailer_vector();
                }

                m_trailer_vector[m_trailer_pointer] = node;
                m_trailer_pointer++;
            }
        }

        template <typename>
        static constexpr bool always_false_v = false;

        OutgoingFixMessage(const OutgoingFixMessage& other) = delete;
        OutgoingFixMessage& operator= (const OutgoingFixMessage& other) = delete;
        OutgoingFixMessage(OutgoingFixMessage&& other) = delete;
        OutgoingFixMessage& operator=(OutgoingFixMessage&& other) = delete;
};

} // namespace