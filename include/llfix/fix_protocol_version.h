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

namespace llfix
{

namespace FixProtocolVersion
{
    static constexpr int  FIX40 = 40;
    static constexpr int  FIX41 = 41;
    static constexpr int  FIX42 = 42;
    static constexpr int  FIX43 = 43;
    static constexpr int  FIX44 = 44;
    static constexpr int  FIX50 = 50;
} // namespace

inline int begin_string_to_fix_protocol_version(const std::string& val)
{
    int ret{0};

    if      (val == "FIX.4.0")  ret = FixProtocolVersion::FIX40;
    else if (val == "FIX.4.1")  ret = FixProtocolVersion::FIX41;
    else if (val == "FIX.4.2")  ret = FixProtocolVersion::FIX42;
    else if (val == "FIX.4.3")  ret = FixProtocolVersion::FIX43;
    else if (val == "FIX.4.4")  ret = FixProtocolVersion::FIX44;
    else if (val == "FIXT.1.1") ret = FixProtocolVersion::FIX50;

    return ret;
}

} // namespace