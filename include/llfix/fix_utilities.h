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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>

#include <string>
#include <string_view>

#include <immintrin.h>

#ifdef __linux__ // VOLTRON_EXCLUDE
#include <string.h> // memrchr
#endif // VOLTRON_EXCLUDE

#include "core/compiler/hints_branch_predictor.h"
#include "core/compiler/hints_hot_code.h"
#include "core/cpu/simd_attributes.h"
#include "core/os/vdso.h"

#include "core/utilities/converters.h"
#include "core/utilities/std_string_utilities.h"

#include "fix_constants.h"
#include "fix_string.h"

namespace llfix
{

/**
 * @class FixUtilities
 * @brief Utility functions for working with FIX messages.
 *
 * Contains static helper functions
 */
class FixUtilities
{
    public:

        static void get_reject_reason_text(char* target, std::size_t& copied_length, uint32_t reject_reason_code)
        {
            assert(target);
            const char* text = nullptr;

            switch (reject_reason_code)
            {
                case FixConstants::FIX_ERROR_CODE_INVALID_TAG_NUMBER :  text = "Invalid tag number"; break;
                case FixConstants::FIX_ERROR_CODE_REQUIRED_TAG_MISSING :  text = "Required tag missing"; break;
                case FixConstants::FIX_ERROR_CODE_TAG_UNDEFINED_FOR_MSG_TYPE :  text = "Tag not defined for this message type"; break;
                case FixConstants::FIX_ERROR_CODE_UNDEFINED_TAG :  text = "Undefined Tag"; break;
                case FixConstants::FIX_ERROR_CODE_TAG_WITHOUT_VALUE :  text = "Tag specified without a value"; break;
                case FixConstants::FIX_ERROR_CODE_VALUE_INCORRECT_FOR_TAG :  text = "Value is incorrect (out of range) for this tag"; break;
                case FixConstants::FIX_ERROR_CODE_FORMAT_INCORRECT_FOR_TAG :  text = "Incorrect data format for value"; break;
                case FixConstants::FIX_ERROR_CODE_FORMAT_DECRYPTION_PROBLEM :  text = "Decryption problem"; break;
                case FixConstants::FIX_ERROR_CODE_FORMAT_SIGNATURE_PROBLEM :  text = "Signature <89> problem"; break;
                case FixConstants::FIX_ERROR_CODE_COMPID_PROBLEM :  text = "CompID problem"; break;
                case FixConstants::FIX_ERROR_CODE_SENDING_TIME_ACCURACY_PROBLEM : text = "SendingTime <52> accuracy problem"; break;
                case FixConstants::FIX_ERROR_CODE_INVALID_MSG_TYPE : text = "Invalid MsgType <35>"; break;
                case FixConstants::FIX_ERROR_CODE_XML_VALIDATION_ERROR : text = "XML Validation error"; break;
                case FixConstants::FIX_ERROR_CODE_TAG_APPEARS_MORE_THAN_ONCE : text = "Tag appears more than once"; break;
                case FixConstants::FIX_ERROR_CODE_TAG_OUT_OF_ORDER : text = "Tag specified out of required order"; break;
                case FixConstants::FIX_ERROR_CODE_RG_OUT_OF_ORDER : text = "Repeating group fields out of order"; break;
                case FixConstants::FIX_ERROR_CODE_INCORRECT_NUMINGROUP : text = "Incorrect NumInGroup count for repeating group"; break;
                case FixConstants::FIX_ERROR_CODE_NON_BINARY_VALUE_WITH_SOH : text = "Non \"Data\" value includes field delimiter (<SOH> character)"; break;
                case FixConstants::FIX_ERROR_CODE_OTHER: text = "Other"; break;
                default: text = "Other"; break;
            }

            copied_length = strlen(text);
            llfix_builtin_memcpy(target, text, copied_length);
            target[copied_length] = '\0';
        }

        /**
         * @brief Converts a FIX message buffer to a human-readable string.
         *
         * Replaces the FIX delimiter character with '|'.
         *
         * @param buffer Pointer to the FIX message buffer.
         * @param buffer_length Length of the buffer.
         * @return Human-readable string representation of the FIX message.
         */
        static std::string fix_to_human_readible(const char* buffer, std::size_t buffer_length)
        {
            assert(buffer != nullptr && buffer_length > 0 );
            std::string ret;
            ret.reserve(buffer_length);

            for (std::size_t i = 0; i < buffer_length; ++i)
            {
                ret.push_back(buffer[i] == FixConstants::FIX_DELIMITER ? '|' : buffer[i]);
            }

            return ret;
        }

        static bool is_a_non_retransmittable_admin_message_type(FixString* str)
        {
            assert(str);
            assert(str->length());

            if(str->length() != 1)
            {
                return false;
            }

            char single_char_msg_type = str->data()[0];

            switch(single_char_msg_type)
            {
                // All session level messages except rejects based on FIX session layer specs 4.8.5
                case FixConstants::MSG_TYPE_HEARTBEAT: return true;
                case FixConstants::MSG_TYPE_TEST_REQUEST: return true;
                case FixConstants::MSG_TYPE_RESEND_REQUEST: return true;
                case FixConstants::MSG_TYPE_SEQUENCE_RESET: return true;
                case FixConstants::MSG_TYPE_LOGON: return true;
                case FixConstants::MSG_TYPE_LOGOUT: return true;
                default:  return false;
            }
        }

        LLFIX_FORCE_INLINE static void encode_current_time(FixString* target, VDSO::SubsecondPrecision subsecond_precision)
        {
            assert(target);

            static constexpr uint32_t TIME_LENGTHS[] = {
                27, // NANOSECONDS
                24, // MICROSECONDS
                21, // MILLISECONDS
                17  // NONE
            };

            using FuncPtr = void (*)(char*);

            static constexpr FuncPtr FUNC_TABLE[] =
            {
                &VDSO::get_datetime_as_string<true, VDSO::SubsecondPrecision::NANOSECONDS>,
                &VDSO::get_datetime_as_string<true, VDSO::SubsecondPrecision::MICROSECONDS>,
                &VDSO::get_datetime_as_string<true, VDSO::SubsecondPrecision::MILLISECONDS>,
                &VDSO::get_datetime_as_string<true, VDSO::SubsecondPrecision::NONE>
            };

            const auto index = static_cast<std::size_t>(subsecond_precision);

            FUNC_TABLE[index](target->data());
            target->set_length(TIME_LENGTHS[index]);
        }

        LLFIX_FORCE_INLINE static bool is_utc_timestamp_stale(const std::string_view& value, int max_allowed_age_seconds)
        {
            const char* value_buffer = value.data();

            auto to_int_2 = [](const char* s)
                {
                    return (s[0] - '0') * 10 + (s[1] - '0');
                };

            auto to_int_4 = [](const char* s)
                {
                    return (s[0] - '0') * 1000 +
                           (s[1] - '0') * 100 +
                           (s[2] - '0') * 10 +
                           (s[3] - '0');
                };

            std::tm tm{};
            tm.tm_year = to_int_4(value_buffer) - 1900;
            tm.tm_mon  = to_int_2(value_buffer + 4) - 1;
            tm.tm_mday = to_int_2(value_buffer + 6);
            tm.tm_hour = to_int_2(value_buffer + 9);
            tm.tm_min  = to_int_2(value_buffer + 12);
            tm.tm_sec  = to_int_2(value_buffer + 15);

            time_t msg_time;
            #ifdef __linux__
            msg_time = timegm(&tm);
            #elif _WIN32
            msg_time = _mkgmtime(&tm);
            #endif

            if (msg_time == -1)
                return true;

            time_t now = time(nullptr);
            return (now - msg_time) > max_allowed_age_seconds;
        }

        LLFIX_FORCE_INLINE static bool find_delimiter_from_end(char* buffer, std::size_t buffer_size, int& index)
        {
            #ifdef __linux__
            void* p = memrchr(buffer, FixConstants::FIX_DELIMITER, buffer_size);

            if (llfix_unlikely(!p) )
            {
                return false;
            }

            index = static_cast<int>(static_cast<char*>(p) - buffer);
            #elif _WIN32
            while (true)
            {
                if (buffer[index] == FixConstants::FIX_DELIMITER)
                {
                    break;
                }

                if (index == 0)
                {
                    return false;
                }

                index--;
            }
            #endif

            return true;
        }

        LLFIX_FORCE_INLINE static void find_tag10_start_from_end(char* buffer, std::size_t buffer_size, int& index, int& final_tag10_delimiter_index)
        {
            if (buffer_size < 3)
                return;

            const int max_index = static_cast<int>(buffer_size - 3);
            if (index > max_index)
                index = max_index;

            while (true)
            {
                if (buffer[index] == '1' && buffer[index + 1] == '0' && buffer[index + 2] == FixConstants::FIX_EQUALS)
                {
                    // FIND OUT TAG10
                    int temp_index = index + 2;

                    while (true)
                    {
                        if (temp_index == static_cast<int>(buffer_size))
                        {
                            break;
                        }

                        if (buffer[temp_index] == FixConstants::FIX_DELIMITER)
                        {
                            final_tag10_delimiter_index = temp_index;
                            break;
                        }

                        temp_index++;
                    }
                    break;
                }

                if (index == 0)
                {
                    break;
                }

                index--;
            }
        }

        LLFIX_FORCE_INLINE static void find_begin_string_position(char* buffer, std::size_t buffer_size, int& begin_string_position)
        {
            std::size_t current_index = 0;

            while (current_index<buffer_size-1)
            {
                if(buffer[current_index] == '8' && buffer[current_index+1] == FixConstants::FIX_EQUALS)
                {
                    begin_string_position = static_cast<int>(current_index);
                    break;
                }

                current_index++;
            }
        }

        LLFIX_FORCE_INLINE static void encode_checksum_no_simd(const char* buffer, std::size_t buffer_length, char* out)
        {
            assert(out);
            uint32_t sum{ 0 };

            for (std::size_t i = 0; i < buffer_length; ++i)
            {
                sum += static_cast<unsigned char>(buffer[i]);
            }

            const uint32_t checksum = sum % FixConstants::FIX_CHECKSUM_MODULO;

            out[0] = '0' + (checksum / 100);
            out[1] = '0' + ((checksum / 10) % 10);
            out[2] = '0' + (checksum % 10);
        }

        // No aligned address requirement
        LLFIX_SIMD_TARGET_AVX2
        static void encode_checksum_simd_avx2(const char* buffer, std::size_t buffer_length, char* out)
        {
            assert(out);

            uint32_t sum = 0;

            const std::size_t simd_width = 32;
            const __m256i zero = _mm256_setzero_si256();
            __m256i acc = zero;

            std::size_t i = 0;
            for (; i + simd_width <= buffer_length; i += simd_width)
            {
                __m256i bytes = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buffer + i));

                __m256i lo = _mm256_unpacklo_epi8(bytes, zero);
                __m256i hi = _mm256_unpackhi_epi8(bytes, zero);

                acc = _mm256_add_epi16(acc, lo);
                acc = _mm256_add_epi16(acc, hi);
            }

            __m128i acc_lo = _mm256_extracti128_si256(acc, 0);
            __m128i acc_hi = _mm256_extracti128_si256(acc, 1);
            __m128i total = _mm_add_epi16(acc_lo, acc_hi);

            uint16_t temp[8];
            _mm_storeu_si128(reinterpret_cast<__m128i*>(temp), total);

            for (int j = 0; j < 8; ++j)
                sum += temp[j];

            for (; i < buffer_length; ++i)
                sum += static_cast<unsigned char>(buffer[i]);

            const uint32_t checksum = sum % FixConstants::FIX_CHECKSUM_MODULO;

            out[0] = '0' + (checksum / 100);
            out[1] = '0' + ((checksum / 10) % 10);
            out[2] = '0' + (checksum % 10);
        }

        LLFIX_FORCE_INLINE static bool validate_checksum_no_simd(const char* buffer, std::size_t buffer_length, uint32_t actual_checksum)
        {
            uint32_t sum{ 0 };

            for (std::size_t i = 0; i < buffer_length; ++i)
            {
                sum += static_cast<unsigned char>(buffer[i]);
            }

            const uint32_t checksum = sum % FixConstants::FIX_CHECKSUM_MODULO;

            return checksum == actual_checksum;
        }

        // No aligned address requirement
        LLFIX_SIMD_TARGET_AVX2
        static bool validate_checksum_simd_avx2(const char* buffer, std::size_t buffer_length, uint32_t actual_checksum)
        {
            uint32_t sum = 0;

            const std::size_t simd_width = 32;
            const __m256i zero = _mm256_setzero_si256();
            __m256i acc = zero;

            std::size_t i = 0;
            for (; i + simd_width <= buffer_length; i += simd_width)
            {
                __m256i bytes = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buffer + i));

                __m256i lo = _mm256_unpacklo_epi8(bytes, zero);
                __m256i hi = _mm256_unpackhi_epi8(bytes, zero);

                acc = _mm256_add_epi16(acc, lo);
                acc = _mm256_add_epi16(acc, hi);
            }

            __m128i acc_lo = _mm256_extracti128_si256(acc, 0);
            __m128i acc_hi = _mm256_extracti128_si256(acc, 1);
            __m128i total = _mm_add_epi16(acc_lo, acc_hi);

            uint16_t temp[8];
            _mm_storeu_si128(reinterpret_cast<__m128i*>(temp), total);

            for (int j = 0; j < 8; ++j)
                sum += temp[j];

            for (; i < buffer_length; ++i)
                sum += static_cast<unsigned char>(buffer[i]);

            const uint32_t checksum = sum % FixConstants::FIX_CHECKSUM_MODULO;

            return checksum == actual_checksum;
        }

        static uint32_t pack_message_type(const std::string_view& mt)
        {
            assert(mt.size()>0 && mt.size() <= FixConstants::MAX_SUPPORTED_MESSAGE_TYPE_LENGTH);
            uint32_t ret = 0;

            for (std::size_t i = 0; i < mt.size(); ++i)
                ret |= uint32_t(uint8_t(mt[i])) << (i * 8);

            return ret;
        }

        static std::string unpack_message_type(uint32_t encoded_msg_type)
        {
            std::string ret;
            ret.reserve(FixConstants::MAX_SUPPORTED_MESSAGE_TYPE_LENGTH);

            for (std::size_t i = 0; i < FixConstants::MAX_SUPPORTED_MESSAGE_TYPE_LENGTH; ++i)
            {
                char c = char((encoded_msg_type >> (i * 8)) & 0xFF);

                if (c == '\0')
                    break;

                ret.push_back(c);
            }

            return ret;
        }

        // Slow and allocates memory but used only loading existing serialised files
        static uint32_t get_sequence_number_value_from_fix_message(const std::string& buffer)
        {
            auto tag_value_pairs = StringUtilities::split(buffer, FixConstants::FIX_DELIMITER);

            for (const auto& tag_value_pair : tag_value_pairs)
            {
                if (tag_value_pair.length() > 3)
                {
                    if (tag_value_pair[0] == '3' && tag_value_pair[1] == '4' && tag_value_pair[2] == FixConstants::FIX_EQUALS)
                    {
                        return Converters::chars_to_unsigned_int<uint32_t>(&tag_value_pair[3], tag_value_pair.length() - 3);
                    }
                }
            }

            return 0;
        }
};

// Wrapper for get_sequence_number_value_from_fix_message
struct FixMessageSequenceNumberExtractor
{
    static uint32_t get_sequence_number_from_message(const std::string& message)
    {
        return FixUtilities::get_sequence_number_value_from_fix_message(message);
    }
};

} // namespace