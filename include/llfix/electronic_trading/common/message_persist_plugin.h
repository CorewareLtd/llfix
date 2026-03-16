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

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace llfix
{

/**
 * @brief Interface for custom FIX message persistence plugins.
 *
 * @details
 * MessagePersistPlugin allows users to implement custom persistence logic
 * for FIX messages
 *
 * To create a custom message persister, derive from this interface and
 * implement both persist methods:
 *  - persist_incoming_message()
 *  - persist_outgoing_message()
 *
 * The plugin can be enabled by calling:
 *  - llfix::FixClient::set_message_persist_plugin(), or
 *  - llfix::FixServer::set_message_persist_plugin()
 */
class MessagePersistPlugin
{
    public:

        static constexpr uint32_t INTERFACE_VERSION = 1;

        virtual ~MessagePersistPlugin() = default;

        /**
         * @brief Persists an incoming FIX message.
         *
         * @details
         * This method is invoked for every FIX message received by the engine
         *
         * @param session_name Logical FIX session name.
         * @param sequence_number FIX message sequence number.
         * @param buffer Pointer to the raw FIX message buffer.
         * @param buffer_size Size of the FIX message buffer in bytes.
         *
         * @note
         * The buffer is only valid for the duration of this call and must not
         * be stored directly without copying.
         */
        virtual void persist_incoming_message(const std::string_view& session_name, uint32_t sequence_number, const char* buffer, std::size_t buffer_size) = 0;

        /**
         * @brief Persists an outgoing FIX message.
         *
         * @details
         * This method is invoked for every FIX message sent by the engine.
         * The persistence callback includes information about whether the
         * message was successfully transmitted.
         *
         * @param session_name Logical FIX session name.
         * @param sequence_number FIX message sequence number.
         * @param buffer Pointer to the raw FIX message buffer.
         * @param buffer_size Size of the FIX message buffer in bytes.
         * @param successfully_transmitted Indicates whether the message was
         *        successfully sent to the peer.
         *
         * @note
         * The buffer is only valid for the duration of this call and must not
         * be stored directly without copying.
         */
        virtual void persist_outgoing_message(const std::string_view& session_name, uint32_t sequence_number, const char* buffer, std::size_t buffer_size, bool successfully_transmitted) = 0;
};

}