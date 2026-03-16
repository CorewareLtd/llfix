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

#include <array>
#include <string_view>

namespace llfix
{

namespace Commands
{

enum CommandId
{
    // GENERAL
    HELP,
    GET_UPTIME,
    // ENGINE
    GET_ENGINE_VERSION,
    GET_ENGINE_LOG_PATH,
    GET_ENGINE_LOG_LEVEL,
    SET_ENGINE_LOG_LEVEL,
    // INSTANCES
    GET_CLIENTS,
    GET_SERVERS,
    GET_INSTANCE_CONFIG,
    // INSTANCE HIGH AVAILABILITY
    SET_IS_INSTANCE_PRIMARY,
    IS_INSTANCE_PRIMARY,
    // SESSION
    GET_SESSIONS,
    GET_SESSION_STATE,
    GET_SESSION_CONFIG,
    GET_ALL_SESSION_STATES,
    ENABLE_SESSION,
    DISABLE_SESSION,
    // SESSION SEQUENCE NUMBER
    GET_INCOMING_SEQUENCE_NUMBER,
    GET_OUTGOING_SEQUENCE_NUMBER,
    SET_INCOMING_SEQUENCE_NUMBER,
    SET_OUTGOING_SEQUENCE_NUMBER,
    SEND_SEQUENCE_RESET,
    // COMMAND COUNT
    COMMAND_COUNT
};

static constexpr std::array<std::string_view, COMMAND_COUNT> Table =
{
    // GENERAL
    "help",
    "get_uptime",
    // ENGINE
    "get_engine_version",
    "get_engine_log_path",
    "get_engine_log_level",
    "set_engine_log_level",
    // INSTANCES
    "get_clients",
    "get_servers",
    "get_instance_config",
    // INSTANCE HIGH AVAILABILITY
    "set_is_instance_primary",
    "is_instance_primary",
    // SESSION
    "get_sessions",
    "get_session_state",
    "get_session_config",
    "get_all_session_states",
    "enable_session",
    "disable_session",
    // SESSION SEQUENCE NUMBER
    "get_incoming_sequence_number",
    "get_outgoing_sequence_number",
    "set_incoming_sequence_number",
    "set_outgoing_sequence_number",
    "send_sequence_reset"
};

static constexpr char COMMAND_DELIMITER = '|';

}

} // namespace