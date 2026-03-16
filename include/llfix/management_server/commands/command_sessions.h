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

namespace llfix
{

class CommandSessions : public Command
{
    public:
        std::string process(ManagementContext& context) override
        {
            if (m_parameters.size() < 1)
            {
                return "Missing client or server name parameter";
            }

            std::string ret;

            ////////////////////////////////////////////////////////////////////////////
            auto instance = context.get_instance(m_parameters[0]);

            if (instance)
            {
                std::vector<std::string> session_names;
                instance->get_session_names(session_names);

                for (const auto& item : session_names)
                {
                    ret += item + ",";
                }
            }
            ////////////////////////////////////////////////////////////////////////////
            if (ret.length() == 0)
            {
                ret = "No results";
            }

            return ret;
        }
};

} // namespace