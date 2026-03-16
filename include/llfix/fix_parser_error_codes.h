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
#include <string>

namespace llfix
{

// Internal error codes used during parsing
// They start from 100 to avoid clashing with t373 values
namespace FixParserErrorCodes
{
    static inline constexpr uint32_t WRONG_BODY_LENGTH = 100;
    static inline constexpr uint32_t NO_TAG_35 = 101;
    static inline constexpr uint32_t NO_TAG_9 = 102;
    static inline constexpr uint32_t INVALID_TAG_9 = 103;
    static inline constexpr uint32_t NO_EQUALS_SIGN = 104;
    static inline constexpr uint32_t OUT_OF_ORDER_HEADER_FIELDS = 105;

    inline std::string get_internal_parser_error_description(uint32_t reject_reason_code)
    {
        std::string ret;

        switch (reject_reason_code)
        {
            case FixParserErrorCodes::WRONG_BODY_LENGTH: ret = "received a message with wrong body length"; break;
            case FixParserErrorCodes::NO_TAG_35: ret = "received a message with no tag35 (msgtype)"; break;
            case FixParserErrorCodes::NO_TAG_9: ret = "received a message with no tag9 (body length)"; break;
            case FixParserErrorCodes::INVALID_TAG_9: ret = "received a message with invalid tag9 (body length)"; break;
            case FixParserErrorCodes::NO_EQUALS_SIGN: ret = "received a message with invalid tag-value (no equals sign)"; break;
            case FixParserErrorCodes::OUT_OF_ORDER_HEADER_FIELDS: ret = "received a message with header fields out of order"; break;
        }

        return ret;
    }
} // namespace

} // namespace