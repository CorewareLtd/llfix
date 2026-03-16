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
#include "../command.h"
#include "../management_context.h"
#include "../../core/utilities/logger.h"
#include "../../core/compiler/unused.h"

namespace llfix
{

class CommandEngineGetLogLevel : public Command
{
    public:
        std::string process(ManagementContext& context) override
        {
            LLFIX_UNUSED(context);

            auto log_level = Logger<>::get_instance().get_log_level();
            std::string ret;

            switch (log_level)
            {
                case LogLevel::LEVEL_FATAL: ret = "FATAL"; break;
                case LogLevel::LEVEL_ERROR: ret = "ERROR"; break;
                case LogLevel::LEVEL_WARNING: ret = "WARNING"; break;
                case LogLevel::LEVEL_INFO: ret = "INFO"; break;
                case LogLevel::LEVEL_DEBUG: ret = "DEBUG"; break;
                default: ret = "NONE"; break;
            }

            return ret;
        }
};

} // namespace