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
#include <cstdint>
#include <cstddef>
#include <string>

#include "../../core/utilities/converters.h"

namespace llfix
{

 /**
 * @class FixedPoint
 * @brief Represents an unsigned fixed-point numeric value with a configurable number of decimal points. Not general purpose, intended usage is for only exchange connectivity.
 *
 * - Decimal points represent the scale factor.
 *   For example: in `10.32`, decimal points = 2.
 *
 * - If decimal points is 4, a raw value of `10000` represents `1.0000`.
 *
 * - Some common decimal point examples used by exchanges:
 *   - 4  (Nasdaq OMX)
 *   - 6  (Euronext)
 *   - 7  (Euronext)
 *   - 8  (Xetra & Eurex)
 *
 * - Raw value is stored as a 64-bit unsigned integer, matching typical exchange-native binary protocol fields (e.g. price).
 *
 * - Arithmetic operations not supported. You can use get_raw_value and set_raw_value to modify the underlying value
 *
 * @warning
 * 1. You must not use a FixedPoint instance without setting decimal points.
 * 2. set_from_chars assumes a strictly valid unsigned decimal representation. Accepted format: digits with an optional single '.' delimiter (e.g. "123", "123.45").
 *    The parser does not validate or skip whitespace, signs, separators, exponent notation, or multiple '.' characters.
 *    Any non-digit (other than a single '.') or multiple '.' occurrences yield undefined/implementation-specific results.
 *
 * @section fixedpoint_usage Usage
 *
 * @subsection fixedpoint_usage_1 Example 1
 * @code
 * FixedPoint fp;
 * fp.set_decimal_points(4);
 * fp.set_from_chars("1000.0023");
 * std::cout << fp.to_string();      // 1000.0023
 * std::cout << fp.get_raw_value();  // 10000023
 * @endcode
 *
 * @subsection fixedpoint_usage_2 Example 2
 * @code
 * FixedPoint fp;
 * fp.set_decimal_points(4);
 * fp.set_raw_value(10001234);
 * std::cout << fp.to_string();     // 1000.1234
 * std::cout << fp.get_raw_value(); // 10001234
 * @endcode
 */
class FixedPoint
{
public:

    /**
     * @brief Set the number of decimal points for this value.
     * @param n Number of decimal points (max allowed value is 9)
     */
    void set_decimal_points(uint32_t n)
    {
        assert(n>0 && n <= MAX_ALLOWED_DECIMAL_POINTS);
        m_decimal_points = n;
    }

    /**
     * @brief Get the number of decimal points for this value.
     * @return Number of decimal points
     */
    uint32_t get_decimal_points() const
    {
        return m_decimal_points;
    }

    /**
     * @brief Set the raw integer value.
     * @param raw_value The raw value corresponding to the fixed-point representation
     */
    void set_raw_value(uint64_t raw_value)
    {
        m_raw_value = raw_value;
    }

    /**
     * @brief Get the raw integer value.
     * @return Raw fixed-point value
     */
    uint64_t get_raw_value() const
    {
        return m_raw_value;
    }

    /**
     * @brief Set value from a character buffer representing a decimal number.
     *
     * Example: if decimal points = 4, input "10.2345" -> raw value = 102345
     *
     * @param buffer Character buffer containing the numeric value
     * @param length Length of the buffer
     *
     */
    void set_from_chars(const char* buffer, std::size_t length)
    {
        assert(m_decimal_points >0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);
        assert(buffer != nullptr);
        assert(length > 0);

        char temp_buffer[MAX_DIGITS];
        std::size_t temp_buffer_length = 0;

        std::size_t length_fractal_part = 0;
        bool in_fractal_part = false;

        for (std::size_t i = 0; i < length; i++)
        {
            char current_char = buffer[i];

            if (current_char != '.')
            {
                assert(temp_buffer_length < MAX_DIGITS);

                temp_buffer[temp_buffer_length] = current_char;
                temp_buffer_length++;

                if (in_fractal_part )
                {
                    length_fractal_part++;
                }
            }
            else
            {
                in_fractal_part = true;
            }
        }

        assert(length_fractal_part <= m_decimal_points && length_fractal_part <= MAX_ALLOWED_DECIMAL_POINTS);

        uint64_t scale = POWERS_OF_TEN[m_decimal_points];
        scale /= static_cast<uint64_t>(POWERS_OF_TEN[length_fractal_part]);

        auto value = Converters::chars_to_unsigned_int<uint64_t>(temp_buffer, temp_buffer_length);

        m_raw_value = static_cast<uint64_t>(value * scale);
    }

    /**
     * @brief Convert the fixed-point value into characters in buffer.
     *
     * Example: raw value 102345 with decimal points 4 -> buffer "10.2345"
     *
     * @param buffer Target buffer to write characters
     * @return Number of characters written
     *
     * @note The output is NOT null-terminated; callers must append '\0' if a C-string is required.
     */
    std::size_t to_chars(char* buffer) const
    {
        assert(m_decimal_points >0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);
        assert(buffer != nullptr);

        char temp_buffer[MAX_DIGITS];
        Converters::unsigned_int_to_chars<uint64_t>(m_raw_value, temp_buffer);

        std::size_t length = Converters::get_chars_length_of_uint64_t(m_raw_value);

        assert(length > 0);

        if (length > m_decimal_points)
        {
            assert(length + 1 < MAX_DIGITS);

            std::size_t integer_part_length = length - m_decimal_points;

            // Copy integer part
            for (std::size_t i = 0; i < integer_part_length; i++)
            {
                buffer[i] = temp_buffer[i];
            }

            buffer[integer_part_length] = '.';

            // Copy fractal part
            for (std::size_t i = 0; i < m_decimal_points; i++)
            {
                buffer[integer_part_length + 1 + i] = temp_buffer[integer_part_length + i];
            }

            return length + 1; // +1 is because of '.'
        }
        else
        {
            assert(2 + m_decimal_points < MAX_DIGITS);

            buffer[0] = '0';
            buffer[1] = '.';

            std::size_t max_buffer_index = 2 + m_decimal_points - 1;

            for (std::size_t i = 0; i < length; i++)
            {
                buffer[max_buffer_index - i] = temp_buffer[length-1-i];
            }

            for (std::size_t i = 0; i < m_decimal_points-length; i++)
            {
                buffer[2 + i] = '0';
            }

            return 2 + m_decimal_points; // +2 is because of "0."
        }
    }

    /**
     * @brief Convert the fixed-point value to a std::string.
     * @return String representation including decimal point
     */
    std::string to_string() const
    {
        assert(m_decimal_points >0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);

        char temp_buffer[MAX_DIGITS];
        auto length = to_chars(temp_buffer);
        temp_buffer[length] = '\0';

        std::string ret = temp_buffer;
        return ret;
    }

    /**
     * @brief Equality comparison
     */
    bool operator ==(const FixedPoint& other) const
    {
        assert(m_decimal_points > 0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);
        assert(m_decimal_points == other.m_decimal_points);

        return m_raw_value == other.m_raw_value;
    }

    /**
     * @brief Inequality comparison
     */
    bool operator !=(const FixedPoint& other) const
    {
        assert(m_decimal_points > 0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);
        assert(m_decimal_points == other.m_decimal_points);

        return m_raw_value != other.m_raw_value;
    }

    /**
     * @brief Greater-than comparison
     */
    bool operator >(const FixedPoint& other) const
    {
        assert(m_decimal_points > 0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);
        assert(m_decimal_points == other.m_decimal_points);
        return m_raw_value > other.m_raw_value;
    }

    /**
     * @brief Greater-than-or-equal comparison
     */
    bool operator >=(const FixedPoint& other) const
    {
        assert(m_decimal_points > 0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);
        assert(m_decimal_points == other.m_decimal_points);
        return m_raw_value >= other.m_raw_value;
    }

    /**
     * @brief Less-than comparison
     */
    bool operator <(const FixedPoint& other) const
    {
        assert(m_decimal_points > 0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);
        assert(m_decimal_points == other.m_decimal_points);
        return m_raw_value < other.m_raw_value;
    }

    /**
     * @brief Less-than-or-equal comparison
     */
    bool operator <=(const FixedPoint& other) const
    {
        assert(m_decimal_points > 0 && m_decimal_points <= MAX_ALLOWED_DECIMAL_POINTS);
        assert(m_decimal_points == other.m_decimal_points);
        return m_raw_value <= other.m_raw_value;
    }

private:
    uint64_t m_raw_value = 0;
    uint32_t m_decimal_points = 0;

    static constexpr inline std::size_t MAX_DIGITS = 64;
    static constexpr inline std::size_t MAX_ALLOWED_DECIMAL_POINTS = 9;

    static constexpr inline uint64_t POWERS_OF_TEN[] =
    {
        UINT64_C(1),          UINT64_C(10),       UINT64_C(100),
        UINT64_C(1000),       UINT64_C(10000),    UINT64_C(100000),
        UINT64_C(1000000),    UINT64_C(10000000), UINT64_C(100000000),
        UINT64_C(1000000000),
    };
};

} // namespace