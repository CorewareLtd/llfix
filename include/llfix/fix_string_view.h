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

// ONLY TO BE USED FOR INCOMING MESSAGES

#include <cassert>
#include <cstring>
#include <cstddef>
#include <string>
#include <string_view>

#include "fix_constants.h"

namespace llfix
{

class FixStringView
{
    public:
        FixStringView() = default;
        ~FixStringView() = default;

        void set_buffer(char* address, std::size_t length)
        {
            assert(address != nullptr);
            m_buffer = address;
            m_length = length;
        }

        std::size_t length() const
        {
            return m_length;
        }

        std::size_t size() const
        {
            return m_length;
        }

        char* data()
        {
            return m_buffer;
        }

        const char* c_str()
        {
            return m_buffer;
        }

        std::string to_string() const
        {
            std::string ret;
            for (std::size_t i = 0; i < m_length; i++)
            {
                ret += m_buffer[i];
            }

            return ret;
        }

        std::string_view to_string_view() const
        {
            return std::string_view(m_buffer, m_length);
        }

        bool equals(const char* other) const
        {
            assert(m_length>0 && m_buffer);
            return std::strlen(other) == m_length && strncmp(m_buffer, other, m_length) == 0;
        }

        bool is_numeric() const
        {
            if (m_length == 0)
            {
                return false;
            }

            std::size_t start = 0;
            const char first = m_buffer[0];
            if (first == '+' || first == '-')
            {
                if (m_length == 1)
                {
                    return false;
                }
                start = 1;
            }

            for (std::size_t i = start; i < m_length; ++i)
            {
                if (m_buffer[i] < '0' || m_buffer[i] > '9')
                {
                    return false;
                }
            }
            return true;
        }

        bool is_double() const
        {
            if (m_length == 0)
            {
                return false;
            }

            std::size_t start = 0;
            const char first = m_buffer[0];
            if (first == '+' || first == '-')
            {
                if (m_length == 1)
                {
                    return false;
                }
                start = 1;
            }

            for (std::size_t i = start; i < m_length; ++i)
            {
                const char c = m_buffer[i];

                if ((c < '0' || c > '9') && c != '.')
                {
                    return false;
                }
            }

            return true;
        }

        bool is_boolean() const
        {
            if (m_length != 1)
            {
                return false;
            }

            const char c = m_buffer[0];

            if (c == FixConstants::FIX_BOOLEAN_TRUE || c == FixConstants::FIX_BOOLEAN_FALSE)
            {
                return true;
            }

            return false;
        }

        bool is_timestamp() const
        {
            // Format: YYYYMMDD-HH:MM:SS or YYYYMMDD-HH:MM:SS.<subseconds>
            if (m_length < 17) // minimal length
                return false;

            if (m_buffer[8] != '-' || m_buffer[11] != ':' || m_buffer[14] != ':')
                return false;

            // Check date
            for (int i = 0; i < 8; ++i)
                if (m_buffer[i] < '0' || m_buffer[i] > '9')
                    return false;

            // Check time
            const int array_time[] = { 9, 10, 12, 13, 15, 16 };
            for (int i : array_time)
                if (m_buffer[i] < '0' || m_buffer[i] > '9')
                    return false;

            // Optional subseconds
            if (m_length > 17)
            {
                if (m_buffer[17] != '.') return false;

                for (std::size_t i = 18; i < m_length; ++i)
                    if (m_buffer[i] < '0' || m_buffer[i] > '9')
                        return false;
            }

            auto to_int = [&](int pos)
            {
                return (m_buffer[pos] - '0') * 10 + (m_buffer[pos+1] - '0');
            };

            // Value ranges
            const int month = to_int(4);
            const int day = to_int(6);
            const int hour = to_int(9);
            const int minutes = to_int(12);
            const int seconds = to_int(15);

            if (hour > 23) return false;
            if (minutes > 59) return false;
            if (seconds > 59) return false;
            if (month < 1 || month > 12) return false;
            if (day < 1 || day > 31)     return false;

            return true;
        }

        bool is_time_only() const
        {
            // Format: HH:MM:SS or HH:MM:SS.<subseconds>
            if (m_length < 8) return false;

            if (m_buffer[2] != ':' || m_buffer[5] != ':')
                return false;

            auto to_int = [&](int pos)
            {
                return (m_buffer[pos] - '0') * 10 + (m_buffer[pos+1] - '0');
            };

            const int array_time[] = { 0, 1, 3, 4, 6, 7 };
            for (int i : array_time)
                if (m_buffer[i] < '0' || m_buffer[i] > '9')
                    return false;

            // Optional subseconds
            if (m_length > 8)
            {
                if (m_buffer[8] != '.') return false;

                for (std::size_t i = 9; i < m_length; ++i)
                    if (m_buffer[i] < '0' || m_buffer[i] > '9')
                        return false;
            }

            // Value ranges
            const int hour = to_int(0);
            const int minutes = to_int(3);
            const int seconds = to_int(6);

            if (hour > 23) return false;
            if (minutes > 59) return false;
            if (seconds > 59) return false;

            return true;
        }

        bool is_date_only() const
        {
            auto to_int = [&](int pos)
            {
                return (m_buffer[pos] - '0') * 10 + (m_buffer[pos+1] - '0');
            };

            // Format: YYYYMMDD
            if (m_length != 8)
                return false;

            for (std::size_t i = 0; i < 8; ++i)
                if (m_buffer[i] < '0' || m_buffer[i] > '9')
                    return false;

            // Value ranges
            const int month = to_int(4);
            const int day = to_int(6);

            if (month < 1 || month > 12) return false;
            if (day < 1 || day > 31)     return false;

            return true;
        }

    private:
        char* m_buffer = nullptr;
        std::size_t m_length = 0;

        FixStringView(const FixStringView& other) = delete;
        FixStringView& operator= (const FixStringView& other) = delete;
        FixStringView(FixStringView&& other) = delete;
        FixStringView& operator=(FixStringView&& other) = delete;
};

} // namespace