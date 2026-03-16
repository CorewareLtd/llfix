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

class CommandEngineSetLogLevel : public Command
{
    public:
        std::string process(ManagementContext& context) override
        {
            LLFIX_UNUSED(context);

            if (m_parameters.size() < 1)
            {
                return "Missing parameter(s)";
            }

            auto log_level = Logger<>::convert_string_to_log_level(m_parameters[0]);

            Logger<>::get_instance().set_log_level(log_level);

            return "Successfully set the engine log level";
        }
};

} // namespace