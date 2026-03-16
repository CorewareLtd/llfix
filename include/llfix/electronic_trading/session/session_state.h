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

/**
 * @enum SessionState
 * @brief Represents the lifecycle and connection state of a session.
 */
enum class SessionState
{
    /** Session has not been initialised. */
    NONE,

    /** Session is administratively disabled. */
    DISABLED,

    /** Session is not connected to the peer. */
    DISCONNECTED,

    /** Session is disconnected after a successful logout. */
    LOGGED_OUT,

    /** Logon attempt was rejected by the peer. */
    LOGON_REJECTED,

    /** TCP connection established, waiting to initiate logon. */
    PENDING_CONNECTION,

    /** Logon message sent, awaiting logon response. */
    PENDING_LOGON,

    /** Logout message sent or received, awaiting completion. */
    PENDING_LOGOUT,

    // LIVE STATES

    /** Session is logged on and fully operational. */
    LOGGED_ON,

    /** Session is retransmitting messages initiated by this side. */
    IN_RETRANSMISSION_INITIATED_BY_SELF,

    /** Session is retransmitting messages initiated by the peer. */
    IN_RETRANSMISSION_INITIATED_BY_PEER
};

inline std::string convert_session_state_to_string(const SessionState& state)
{
    switch (state)
    {
        case SessionState::LOGGED_ON:
            return "Logged on";
        case SessionState::DISCONNECTED:
            return "Disconnected";
        case SessionState::PENDING_CONNECTION:
            return "Pending connection";
        case SessionState::PENDING_LOGON:
            return "Pending logon";
        case SessionState::LOGGED_OUT:
            return "Logged out";
        case SessionState::LOGON_REJECTED:
            return "Logon rejected";
        case SessionState::PENDING_LOGOUT:
            return "Pending logout";
        case SessionState::IN_RETRANSMISSION_INITIATED_BY_SELF:
            return "In retransmission initiated by self";
        case SessionState::IN_RETRANSMISSION_INITIATED_BY_PEER:
            return "In retransmission initiated by peer";
        case SessionState::DISABLED:
            return "Disabled";
        case SessionState::NONE:
            return "None";
        default:
            return "Invalid state";
    }
}

inline bool is_state_live(const SessionState& state)
{
    return state >= SessionState::LOGGED_ON;
}

} // namespace