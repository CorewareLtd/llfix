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

class CommandSetOutgoingSequenceNumber : public Command
{
    public:
        std::string process(ManagementContext& context) override
        {
            if (m_parameters.size() < 3)
            {
                return "Missing parameter(s)";
            }

            if (m_parameters[2].find('-') != std::string::npos)
            {
                return std::string("No results");
            }

            uint32_t new_seq_no = 0;

            try
            {
                new_seq_no = std::stoi(m_parameters[2]);
            }
            catch(...)
            {
                return std::string("No results");
            }

            std::string ret;
            ////////////////////////////////////////////////////////////////////////////
            auto session = context.get_session(m_parameters[0], m_parameters[1]);

            if (session)
            {
                auto instance = context.get_instance(m_parameters[0]);

                if (instance)
                {
                    instance->push_admin_command(m_parameters[1], ModifyingAdminCommandType::SET_OUTGOING_SEQUENCE_NUMBER, new_seq_no);
                    ret = "Successfully pushed to the admin commands queue";
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