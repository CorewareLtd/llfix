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
#include "../../electronic_trading/managed_instance/modifying_admin_command.h"

namespace llfix
{

class CommandSetIsInstancePrimary : public Command
{
    public:
        std::string process(ManagementContext& context) override
        {
            if (m_parameters.size() < 2)
            {
                return "Missing parameter(s)";
            }

            uint32_t val = 0;

            try
            {
                val = std::stoi(m_parameters[1]);
            }
            catch(...)
            {
                return "";
            }

            ////////////////////////////////////////////////////////////////////////////
            auto instance = context.get_instance(m_parameters[0]);

            if(instance && val <= 1)
            {
                instance->push_admin_command("*", ModifyingAdminCommandType::SET_IS_HA_PRIMARY_INSTANCE, val); // * means all sessions
            }

            return ""; // Not returning any response as it can be fired multiple times during a failover
                       // So the receiver should fire is_instance_primary command to check its result
        }
};

} // namespace