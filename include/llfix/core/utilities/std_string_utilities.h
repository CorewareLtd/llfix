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

#include <string>
#include <string_view>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>

namespace llfix
{

class StringUtilities
{
    public:

        template <typename T>
        static T to_lower(const T& input)
        {
            T ret = input;
            std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
            return ret;
        }

        static bool contains(const std::string_view& haystack, const std::string_view& needle)
        {
            if (haystack.find(needle) != std::string::npos)
            {
                return true;
            }

            return false;
        }

        static std::vector<std::string> split(const std::string_view& input, char separator, unsigned int expected_max_token_count = 0)
        {
            std::vector<std::string> ret;

            if(expected_max_token_count > 0)
            {
                ret.reserve(expected_max_token_count);
            }

            if (input.length() > 0)
            {
                std::istringstream stream(input.data());
                std::string token;
                while (std::getline(stream, token, separator))
                {
                    ret.push_back(token);
                }
            }
            return ret;
        }
};

} // namespace