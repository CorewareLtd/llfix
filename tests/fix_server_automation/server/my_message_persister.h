#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

#include <string>

#include <llfix/electronic_trading/common/message_persist_plugin.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/fix_utilities.h>
#include <llfix/core/compiler/unused.h>

class MyMessagePersister : public llfix::MessagePersistPlugin
{
    public:

        void persist_incoming_message(const std::string_view& session_name, uint32_t sequence_number, const char* buffer, std::size_t buffer_size) override
        {
            LLFIX_UNUSED(sequence_number);
            if (llfix::MessagePersistPlugin::INTERFACE_VERSION >= 1)
            {
                llfix::FileSystemUtilities::append_text_to_file(m_incoming_target, session_name.data());
                llfix::FileSystemUtilities::append_text_to_file(m_incoming_target, "-> ");
                llfix::FileSystemUtilities::append_text_to_file(m_incoming_target, llfix::FixUtilities::fix_to_human_readible(buffer, buffer_size)); // fix_to_human_readible converts FIX delimiter to '|' char
                llfix::FileSystemUtilities::append_text_to_file(m_incoming_target, "\n");
            }
        }

        void persist_outgoing_message(const std::string_view& session_name, uint32_t sequence_number, const char* buffer, std::size_t buffer_size, bool successfully_transmitted) override
        {
            LLFIX_UNUSED(sequence_number);
            LLFIX_UNUSED(successfully_transmitted);
            if (llfix::MessagePersistPlugin::INTERFACE_VERSION >= 1)
            {
                llfix::FileSystemUtilities::append_text_to_file(m_outgoing_target, session_name.data());
                llfix::FileSystemUtilities::append_text_to_file(m_outgoing_target, "-> ");
                llfix::FileSystemUtilities::append_text_to_file(m_outgoing_target, llfix::FixUtilities::fix_to_human_readible(buffer, buffer_size)); // fix_to_human_readible converts FIX delimiter to '|' char
                llfix::FileSystemUtilities::append_text_to_file(m_outgoing_target, "\n");
            }
        }

    private:
        std::string m_incoming_target = "incoming.txt";
        std::string m_outgoing_target = "outgoing.txt";
};