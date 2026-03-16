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

#include "../../core/compiler/unused.h"

namespace llfix
{

class CommandHelp : public Command
{
    public:
        std::string process(ManagementContext& context) override
        {
            LLFIX_UNUSED(context);
            std::string ret = R"(Available commands :

            help
            get_uptime

            get_engine_version
            get_engine_log_path
            get_engine_log_level
            set_engine_log_level <log_level_string>

            get_clients
            get_servers
            get_instance_config <client or server name>

            set_is_instance_primary <client or server name> <1 or 0>
            is_instance_primary <client or server name>

            get_sessions <client or server name>
            get_session_state <client or server name> <session_name>
            get_session_config <client or server name> <session_name>
            get_all_session_states <client or server name>
            enable_session <client or server name> <session_name>
            disable_session <client or server name> <session_name>

            get_incoming_sequence_number <client or server name> <session_name>
            get_outgoing_sequence_number <client or server name> <session_name>
            set_incoming_sequence_number <client or server name> <session_name> <sequence_no>
            set_outgoing_sequence_number <client or server name> <session_name> <sequence_no>
            send_sequence_reset <client or server name> <session_name> <sequence_no>

            )";

            return ret;
        }
};

} // namespace