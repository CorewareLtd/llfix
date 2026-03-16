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
    - No chars null termination

    - For low latency, runtime validations are not performed; inputs are expected to be well-formed
      (no digit checks, no range/overflow checks, no buffer/ptr and bounds checks)

    - Provided ones :

                    get_chars_length_of_uint32_t
                    get_chars_length_of_uint64_t

                    chars_to_unsigned_int
                    unsigned_int_to_chars

                    int_to_chars
                    chars_to_int

                    double_to_chars (precision clamped to 9)
                    chars_to_double (precision clamped to 9)
*/
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cctype>
#include <cmath>

#include "../compiler/builtin_functions.h"
#include "../compiler/hints_hot_code.h"
#include "../compiler/unused.h"

namespace llfix
{

class Converters
{
    public:

        ////////////////////////////////////////////////////////////////////////////////////
        // UINT32_T
        LLFIX_FORCE_INLINE static std::size_t get_chars_length_of_uint32_t(uint32_t value)
        {
            // Implementation is for 32 bit integer only
            const unsigned t = (32 - llfix_builtin_clz(value | 1)) * 1233 >> 12;
            return static_cast<std::size_t>(t - (value < POWERS_OF_TEN[t]) + 1);
        }

        ////////////////////////////////////////////////////////////////////////////////////
        // UINT64_T
        LLFIX_FORCE_INLINE static std::size_t get_chars_length_of_uint64_t(uint64_t value)
        {
            // Implementation is for 64 bit integer only
            const unsigned t = (64 - llfix_builtin_clzl(static_cast<uint64_t>(value | 1))) * 1233 >> 12;
            return static_cast<std::size_t>(t - (value < POWERS_OF_TEN_64[t]) + 1);
        }

        ////////////////////////////////////////////////////////////////////////////////////
        // UNSIGNED INT BOTH 32 AND 64 BITS
        template<typename T>
        static T chars_to_unsigned_int(const char* buffer, std::size_t buffer_length)
        {
            static_assert( sizeof(T) == 4 || sizeof(T) == 8);

            T result = 0;
            std::size_t start = 0;

            if (buffer_length > 0 && buffer[0] == '+')
            {
                start = 1;
            }

            for (std::size_t i = start; i < buffer_length; i++)
            {
                T digit = buffer[i] - '0';
                result = result * 10 + digit;
            }

            return result;
        }

        template <typename T>
        static std::size_t unsigned_int_to_chars(T number, char* buffer)
        {
            static_assert(sizeof(T) == 4 || sizeof(T) == 8);

            std::size_t length{ 0 };

            if constexpr (sizeof(T) == 4)
            {
                length = get_chars_length_of_uint32_t(number);
            }
            else if constexpr (sizeof(T) == 8)
            {
                length = get_chars_length_of_uint64_t(number);
            }

            assert(length > 0);

            char* target = buffer + length - 1;

            do
            {
                *target-- = '0' + static_cast<char>((number - (number / 10) * 10)); // Mod10
                number /= 10;
            }
            while (number);

            return length;
        }

        ////////////////////////////////////////////////////////////////////////////////////
        // INT
        template <typename T>
        static std::size_t int_to_chars(T number, char* buffer)
        {
            static_assert(sizeof(T) == 4 || sizeof(T) == 8);
            using UnsignedT = std::make_unsigned_t<T>;

            bool negative = number < 0;
            UnsignedT abs_value = negative
                ? static_cast<UnsignedT>(0 - static_cast<UnsignedT>(number))
                : static_cast<UnsignedT>(number);

            std::size_t length{ 0 };

            if constexpr (sizeof(T) == 4)
            {
                length = get_chars_length_of_uint32_t(static_cast<uint32_t>(abs_value));
            }
            else if constexpr (sizeof(T) == 8)
            {
                length = get_chars_length_of_uint64_t(static_cast<uint64_t>(abs_value));
            }

            if (negative)
                ++length;

            assert(length > 0);

            char* target = buffer + length - 1;

            do
            {
                *target-- = '0' + static_cast<char>((abs_value - (abs_value / 10) * 10)); // Mod10
                abs_value /= 10;
            }
            while (abs_value);

            if (negative)
                *target = '-';

            return length;
        }

        template<typename T>
        static T chars_to_int(const char* buffer, std::size_t buffer_length)
        {
            static_assert(sizeof(T) == 4 || sizeof(T) == 8);
            using UnsignedT = std::make_unsigned_t<T>;

            bool negative = false;
            std::size_t start = 0;

            if (buffer_length > 0 && buffer[0] == '-')
            {
                negative = true;
                start = 1;
            }
            else if (buffer_length > 0 && buffer[0] == '+')
            {
                start = 1;
            }

            UnsignedT result = 0;

            for (std::size_t i = start; i < buffer_length; i++)
            {
                UnsignedT digit = static_cast<UnsignedT>(buffer[i] - '0');
                result = result * 10 + digit;
            }

            if (negative)
            {
                return static_cast<T>(0 - result);
            }

            return static_cast<T>(result);
        }
        ////////////////////////////////////////////////////////////////////////////////////
        // DOUBLE
        static std::size_t double_to_chars(double d, char* target_buffer, std::size_t target_buffer_len, std::size_t precision)
        {
            LLFIX_UNUSED(target_buffer_len);
            std::size_t index = 0;

            std::size_t actual_precision = precision > 9 ? 9 : precision;

            if (d < 0.0)
            {
                target_buffer[index++] = '-';
                d = -d;
            }

            uint64_t int_part = static_cast<uint64_t>(std::floor(d));
            double frac_part = d - int_part;

            /////////////////////////////////////////////////////////
            // CONVERT INTEGER PART
            auto int_length = unsigned_int_to_chars<uint64_t>(int_part, target_buffer + index);
            index += int_length;

            /////////////////////////////////////////////////////////
            // DECIMAL POINTS
            if (actual_precision > 0 )
            {
                target_buffer[index++] = '.';
            }
            /////////////////////////////////////////////////////////
            // CONVERT FRACTAL PART
            frac_part *= POWERS_OF_TEN[actual_precision];

            for (std::size_t i = 0; i < actual_precision ; ++i)
            {
                int digit = static_cast<int>(std::fmod(frac_part, 10));
                target_buffer[index + actual_precision - i - 1] = '0' + digit;
                frac_part = std::floor(frac_part / 10);
            }
            index += actual_precision;

            return index;
        }

        static double chars_to_double(const char* buffer, std::size_t buffer_size, std::size_t precision)
        {
            double result = 0.0;
            bool negative = false;
            std::size_t index = 0;
            bool fractional = false;
            double fractional_divider = 1.0;
            std::size_t fractional_digits = 0;

            std::size_t actual_precision = precision > 9 ? 9 : precision;

            if (buffer[index] == '-')
            {
                negative = true;
                index++;
            }

            while (index < buffer_size)
            {
                char c = buffer[index];

                if (c >= '0' && c <= '9')
                {
                    if (fractional)
                    {
                        if (fractional_digits < actual_precision)
                        {
                            fractional_divider *= 10.0;
                            result += (c - '0') / fractional_divider;
                            fractional_digits++;
                        }
                    }
                    else
                    {
                        result = result * 10.0 + (c - '0');
                    }
                }
                else if (c == '.')
                {
                    fractional = true;
                }

                index++;
            }

            if (actual_precision != 0)
            {
                result = std::round(result * POWERS_OF_TEN[actual_precision]) / POWERS_OF_TEN[actual_precision];
            }
            else
            {
                result = std::round(result);
            }

            if (negative)
            {
                result = -result;
            }

            return result;
        }

    private:

        static constexpr uint32_t POWERS_OF_TEN[] =
        {
            UINT32_C(0),          UINT32_C(10),       UINT32_C(100),
            UINT32_C(1000),       UINT32_C(10000),    UINT32_C(100000),
            UINT32_C(1000000),    UINT32_C(10000000), UINT32_C(100000000),
            UINT32_C(1000000000),
        };

        static constexpr uint64_t POWERS_OF_TEN_64[] =
        {
            UINT64_C(0),                     UINT64_C(10),                    UINT64_C(100),
            UINT64_C(1000),                  UINT64_C(10000),                 UINT64_C(100000),
            UINT64_C(1000000),               UINT64_C(10000000),              UINT64_C(100000000),
            UINT64_C(1000000000),            UINT64_C(10000000000),           UINT64_C(100000000000),
            UINT64_C(1000000000000),         UINT64_C(10000000000000),        UINT64_C(100000000000000),
            UINT64_C(1000000000000000),      UINT64_C(10000000000000000),     UINT64_C(100000000000000000),
            UINT64_C(1000000000000000000),   UINT64_C(10000000000000000000),
        };
};

} // namespace