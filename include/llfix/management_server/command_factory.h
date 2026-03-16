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

#include <cstddef>
#include <cstring>
#include <string_view>

#include "command.h"
#include "commands.h"

// GENERAL
#include "commands/command_help.h"
#include "commands/command_uptime.h"
// ENGINE
#include "commands/command_engine_version.h"
#include "commands/command_engine_log_path.h"
#include "commands/command_engine_get_log_level.h"
#include "commands/command_engine_set_log_level.h"
// INSTANCES
#include "commands/command_clients.h"
#include "commands/command_servers.h"
#include "commands/command_get_instance_config.h"
// INSTANCES - HIGH AVAILABILITY
#include "commands/command_is_instance_primary.h"
#include "commands/command_set_is_instance_primary.h"
// SESSIONS
#include "commands/command_sessions.h"
#include "commands/command_get_session_state.h"
#include "commands/command_get_session_config.h"
#include "commands/command_get_all_session_states.h"
#include "commands/command_enable_session.h"
#include "commands/command_disable_session.h"
// SESSIONS - SEQUENCE NUMBERS
#include "commands/command_get_incoming_sequence_number.h"
#include "commands/command_get_outgoing_sequence_number.h"
#include "commands/command_set_incoming_sequence_number.h"
#include "commands/command_set_outgoing_sequence_number.h"
#include "commands/command_send_sequence_reset.h"


namespace llfix
{

class CommandFactory
{
    public:

        static Command* create_command(const char* command_buffer, std::size_t command_buffer_len)
        {
            Command* ret{ nullptr };
            std::size_t current_token_start{ 0 };
            bool found_command = false;

            for (std::size_t i = 0; i < command_buffer_len; i++)
            {
                if (command_buffer[i] == ' ')
                {
                    std::size_t current_token_length = i - current_token_start;
                    const char* current_token_buf = command_buffer + current_token_start;

                    if (found_command == false)
                    {
                        ret = create_command_from_single_token(current_token_buf, current_token_length);

                        if (ret == nullptr)
                        {
                            return ret; // Invalid command
                        }

                        found_command = true;
                    }
                    else
                    {
                        std::string current_param(current_token_buf , current_token_length);
                        ret->add_parameter(current_param);
                    }

                    current_token_start = i + 1;
                }
            }

            if (ret == nullptr)
            {
                // That means entire buffer was a single command
                ret = create_command_from_single_token(command_buffer, command_buffer_len);
            }
            else
            {
                // Add the final param
                std::string last_param{ command_buffer + current_token_start , command_buffer_len - current_token_start };
                ret->add_parameter(last_param);
            }

            return ret;
        }

    private:

        static bool is_specified_command(const char* command_buffer, std::size_t command_buffer_len, std::string_view command_in_question)
        {
            if (command_buffer_len == command_in_question.size() && strncmp(command_buffer, command_in_question.data(), command_buffer_len) == 0)
                return true;
            return false;
        }

        static Command* create_command_from_single_token(const char* command_buffer, std::size_t command_buffer_len)
        {
            if (command_buffer_len == 0)
            {
                return nullptr;
            }

            Command* ret{ nullptr };

            try
            {
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // GENERAL
                if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::HELP]))
                {
                    ret = new CommandHelp();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_UPTIME]))
                {
                    ret = new CommandUptime();
                }
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // ENGINE
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_ENGINE_VERSION]))
                {
                    ret = new CommandEngineVersion();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_ENGINE_LOG_PATH]))
                {
                    ret = new CommandEngineGetLogPath();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_ENGINE_LOG_LEVEL]))
                {
                    ret = new CommandEngineGetLogLevel();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::SET_ENGINE_LOG_LEVEL]))
                {
                    ret = new CommandEngineSetLogLevel();
                }
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // INSTANCES
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_CLIENTS]))
                {
                    ret = new CommandClients();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_SERVERS]))
                {
                    ret = new CommandServers();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_INSTANCE_CONFIG]))
                {
                    ret = new CommandGetInstanceConfig();
                }
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // INSTANCES - HIGH AVAILABILITY
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::SET_IS_INSTANCE_PRIMARY]))
                {
                    ret = new CommandSetIsInstancePrimary();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::IS_INSTANCE_PRIMARY]))
                {
                    ret = new CommandIsInstancePrimary();
                }
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // SESSIONS
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_SESSIONS]))
                {
                    ret = new CommandSessions();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_SESSION_STATE]))
                {
                    ret = new CommandGetSessionState();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_SESSION_CONFIG]))
                {
                    ret = new CommandGetSessionConfig();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_ALL_SESSION_STATES]))
                {
                    ret = new CommandGetAllSessionStates();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::ENABLE_SESSION]))
                {
                    ret = new CommandEnableSession();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::DISABLE_SESSION]))
                {
                    ret = new CommandDisableSession();
                }
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // SESSIONS - SEQUENCE NUMBERS
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_INCOMING_SEQUENCE_NUMBER]))
                {
                    ret = new CommandGetIncomingSequenceNumber();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::GET_OUTGOING_SEQUENCE_NUMBER]))
                {
                    ret = new CommandGetOutgoingSequenceNumber();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::SET_INCOMING_SEQUENCE_NUMBER]))
                {
                    ret = new CommandSetIncomingSequenceNumber();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::SET_OUTGOING_SEQUENCE_NUMBER]))
                {
                    ret = new CommandSetOutgoingSequenceNumber();
                }
                else if (is_specified_command(command_buffer, command_buffer_len, Commands::Table[Commands::SEND_SEQUENCE_RESET]))
                {
                    ret = new CommandSendSequenceReset();
                }
            }
            catch(...)
            {
                ret = nullptr;
            }

            return ret;
        }
};

} // namespace