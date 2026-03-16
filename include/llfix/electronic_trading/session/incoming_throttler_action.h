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
    enum class IncomingThrottlerAction
    {
        NONE,
        WAIT,
        REJECT,
        DISCONNECT
    };

    inline IncomingThrottlerAction string_to_incoming_throttler_action(const std::string& val)
    {
        IncomingThrottlerAction ret= IncomingThrottlerAction::NONE;

        if(val == "WAIT")
        {
            ret = IncomingThrottlerAction::WAIT;
        }
        else if (val == "REJECT")
        {
            ret = IncomingThrottlerAction::REJECT;
        }
        else if (val == "DISCONNECT")
        {
            ret = IncomingThrottlerAction::DISCONNECT;
        }

        return ret;
    }
}