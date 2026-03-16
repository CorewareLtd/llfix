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

#include "std_string_utilities.h"
#include <string>
#include <string_view>

#ifdef LLFIX_CPP20_AVAILABLE // VOLTRON_EXCLUDE
#include <compare>
#endif // VOLTRON_EXCLUDE

namespace llfix
{

class Version
{
    public:

        explicit Version(const char* strval)
        {
            set_from_string(strval);
        }

        explicit Version(const std::string& strval)
        {
            set_from_string(strval);
        }

        bool set_from_string(const std::string_view& strval)
        {
            if (strval.empty())
            {
                return false;
            }

            auto tokens = StringUtilities::split(strval, '.');

            if (tokens.size() != 3)
            {
                return false;
            }

            try // std::stoi may throw exception
            {
                set_major(std::stoi(tokens[0]));
                set_minor(std::stoi(tokens[1]));
                set_revision(std::stoi(tokens[2]));
            }
            catch (...) { return false; }

            return true;
        }

        #ifdef LLFIX_CPP20_AVAILABLE
        auto operator<=>(const Version&) const = default;
        #endif

        unsigned int get_major() const { return m_major; }
        unsigned int get_minor() const { return m_minor; }
        unsigned int get_revision() const { return m_revision; }

        std::string to_string() const
        {
            return std::to_string(m_major) + '.' + std::to_string(m_minor) + '.' + std::to_string(m_revision);
        }

    private:
        unsigned int m_major = 0;
        unsigned int m_minor = 0;
        unsigned int m_revision = 0;

        void set_major(unsigned int val) { m_major = val; }
        void set_minor(unsigned int val) { m_minor = val; }
        void set_revision(unsigned int val) { m_revision = val; }
};

} // namespace