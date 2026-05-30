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

#include "common.h"

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex> // for std::lock_guard
#include <new>

#include "core/compiler/hints_branch_predictor.h"
#include "core/compiler/hints_hot_code.h"
#include "core/compiler/unused.h"
#include "core/os/vdso.h"
#include "core/os/thread_local_storage.h"

#include "core/utilities/configuration.h"
#include "core/utilities/logger.h"
#include "core/utilities/object_cache.h"
#include "core/utilities/std_string_utilities.h"
#include "core/utilities/userspace_spinlock.h"

#include "electronic_trading/common/message_persist_plugin.h"
#include "electronic_trading/managed_instance/modifying_admin_command.h"
#include "electronic_trading/managed_instance/managed_instance.h"

#include "fix_constants.h"
#include "fix_string_view.h"
#include "fix_string.h"
#include "fix_session_settings.h"
#include "fix_utilities.h"
#include "incoming_fix_message.h"
#include "outgoing_fix_message.h"
#include "fix_parser_error_codes.h"
#include "fix_session.h"
#include "fix_server_settings.h"

#include "core/utilities/tcp_reactor_options.h"

namespace llfix
{

// Thread local
struct UnknownSessionContext
{
    IncomingFixMessage m_incoming_fix_message;
    ObjectCache<FixStringView> m_fix_string_view_cache;
};

class FixServerConnectors
{
    public:
        FixServerConnectors()
        {
            m_tables_lock.initialise();
            m_peer_index_session_table.reserve(1024);
            m_session_peer_index_table.reserve(1024);
        }

        bool has_peer(std::size_t peer_index) const
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_tables_lock);

            if (m_peer_index_session_table.find(peer_index) == m_peer_index_session_table.end())
            {
                return false;
            }

            return true;
        }

        void update(std::size_t peer_index, FixSession* session)
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_tables_lock);

            // In concurrent scenarios, disconnection for an existing item can happen before its connection as they will happen on different threads
            if (m_session_peer_index_table.find(session) != m_session_peer_index_table.end())
            {
                auto existing_peer_index = m_session_peer_index_table[session];

                if (existing_peer_index != peer_index)
                {
                    m_peer_index_session_table.erase(existing_peer_index);
                }
            }

            m_peer_index_session_table[peer_index] = session;
            m_session_peer_index_table[session] = peer_index;
        }

        FixSession* get_session(std::size_t peer_index)
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_tables_lock);

            if (m_peer_index_session_table.find(peer_index) == m_peer_index_session_table.end())
            {
                return nullptr;
            }

            return m_peer_index_session_table[peer_index];
        }

        std::size_t get_peer_index(FixSession* session)
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_tables_lock);

            if (m_session_peer_index_table.find(session) == m_session_peer_index_table.end())
            {
                return static_cast<std::size_t>(-1);
            }

            return m_session_peer_index_table[session];
        }

        void remove(std::size_t peer_index, FixSession* session)
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_tables_lock);

            m_peer_index_session_table.erase(peer_index);
            m_session_peer_index_table.erase(session);
        }

    private:
        std::unordered_map<std::size_t, FixSession*> m_peer_index_session_table;
        std::unordered_map<FixSession*, std::size_t> m_session_peer_index_table;
        mutable UserspaceSpinlock<> m_tables_lock;
};

/**
 * @brief FIX server implementation.
 *
 * Manages FIX sessions, accepts incoming client connections,
 * routes FIX messages, and provides virtual callbacks for
 * application-level and session-level message handling.
 *
 * @tparam Transport Transport layer implementation: one of TcpReactor TcpReactorScalable TcpReactorScalableSSL.
 */
template<typename Transport>
class FixServer : public Transport, public ManagedInstance
{
    public:
        FixServer() = default;

        virtual ~FixServer()
        {
            for (auto& entry : m_sessions)
            {
                delete entry.second;
            }
        }

        /**
         * @brief Creates and initialises the FIX server instance.
         *
         * @param server_name Logical name of the FIX server instance.
         * @param server_config_file_path Path to the server configuration file.
         *
         * @return true on successful creation, false on error.
         */
        [[nodiscard]] bool create(const std::string& server_name, const std::string server_config_file_path)
        {
            m_name = server_name;

            if (m_settings.load_from_config_file(server_config_file_path, server_name) == false)
            {
                LLFIX_LOG_ERROR("Loading settings for server " + server_name + " failed : " + m_settings.config_load_error);
                return false;
            }

            if (m_settings.validate() == false)
            {
                LLFIX_LOG_ERROR("FixServerSettings for " + m_name + " validation failed : " + m_settings.validation_error);
                return false;
            }

            m_is_ha_primary = m_settings.starts_as_primary_instance;
            m_unknown_session_context_lock.initialise();

            TCPReactorOptions reactor_options;

            reactor_options.m_accept_timeout_seconds = m_settings.accept_timeout_seconds;
            reactor_options.m_async_io_timeout_nanoseconds = m_settings.async_io_timeout_nanoseconds;
            reactor_options.m_busy_poll_microseconds = m_settings.busy_poll_microseconds;
            reactor_options.m_cpu_core_id = m_settings.cpu_core_id;
            reactor_options.m_worker_thread_count = m_settings.worker_thread_count;
            reactor_options.m_send_try_count = m_settings.send_try_count;
            reactor_options.m_disable_nagle = m_settings.disable_nagle;
            reactor_options.m_enable_quick_ack = m_settings.quick_ack;
            reactor_options.m_max_poll_events = m_settings.max_poll_events;
            reactor_options.m_nic_interface_ip = m_settings.nic_address;
            reactor_options.m_nic_interface_name = m_settings.nic_name;
            reactor_options.m_nic_ringbuffer_rx_size = m_settings.nic_ringbuffer_rx_size;
            reactor_options.m_nic_ringbuffer_tx_size = m_settings.nic_ringbuffer_tx_size;
            reactor_options.m_pending_connection_queue_size = m_settings.pending_connection_queue_size;
            reactor_options.m_port = m_settings.accept_port;
            reactor_options.m_rx_buffer_capacity = m_settings.rx_buffer_capacity;
            reactor_options.m_receive_size = m_settings.receive_size;
            reactor_options.m_socket_rx_size = m_settings.socket_rx_size;
            reactor_options.m_socket_tx_size = m_settings.socket_tx_size;
            reactor_options.m_spin_count = m_settings.spin_count;
            #ifdef LLFIX_ENABLE_OPENSSL
            reactor_options.m_use_ssl = m_settings.use_ssl;
            reactor_options.m_ssl_verify_peer = m_settings.ssl_verify_peer;
            reactor_options.m_ssl_ca_pem_file = m_settings.ssl_ca_pem_file;
            reactor_options.m_ssl_cert_pem_file = m_settings.ssl_certificate_pem_file;
            reactor_options.m_ssl_private_key_pem_file = m_settings.ssl_private_key_pem_file;
            reactor_options.m_ssl_private_key_password = m_settings.ssl_private_key_password;
            reactor_options.m_ssl_version = m_settings.ssl_version;
            reactor_options.m_ssl_cipher_suite = m_settings.ssl_cipher_suite;
            reactor_options.m_ssl_crl_path = m_settings.ssl_crl_path;
            #endif

            this->set_params(reactor_options);

            if constexpr (Transport::is_multithreaded())
            {
                this->register_acceptor_callback(
                    [this](std::size_t peer_index)
                    {
                        LLFIX_UNUSED(peer_index);
                        this->process_admin_commands_of_all_non_live_sessions();
                    });

                this->register_acceptor_termination_callback(
                    [this](std::size_t peer_index)
                    {
                        LLFIX_UNUSED(peer_index);
                        this->destroy_thread_local_unknown_session_context();
                    });

                this->register_worker_callback(
                    [this](std::size_t peer_index)
                    {
                        this->process_session(peer_index);
                    });

                this->register_worker_termination_callback(
                    [this](std::size_t peer_index)
                    {
                        LLFIX_UNUSED(peer_index);
                        this->destroy_thread_local_unknown_session_context();
                    });
            }
            else
            {
                this->register_callback(std::bind(&FixServer::process, this));
                this->register_termination_callback(std::bind(&FixServer::destroy_thread_local_unknown_session_context, this));
            }

            LLFIX_LOG_INFO(m_name + " : Loaded server config =>\n" + m_settings.to_string());
            LLFIX_LOG_INFO("FixServer " + m_name + " creation success");

            return true;
        }

        /**
         * @brief Adds a FIX session using preloaded session settings.
         *
         * @param session_name Logical name of the session.
         * @param session_settings Session configuration settings.
         *
         * @return true if the session was added successfully, false otherwise.
         */
        [[nodiscard]] bool add_session(const std::string& session_name, FixSessionSettings& session_settings)
        {
            return internal_add_session(session_name, session_settings);
        }

        /**
         * @brief Adds a FIX session by loading settings from a configuration file.
         *
         * @param session_config_file_path Path to the session configuration file.
         * @param session_name Name of the session group in the configuration.
         *
         * @return true if the session was added successfully, false otherwise.
         */
        [[nodiscard]] bool add_session(const std::string& session_config_file_path, const std::string& session_name)
        {
            FixSessionSettings session_settings;

            if (session_settings.load_from_config_file(session_config_file_path, session_name) == false)
            {
                LLFIX_LOG_ERROR("Loading settings for session " + session_name + " failed : " + session_settings.config_load_error);
                return false;
            }

            return internal_add_session(session_name, session_settings);
        }

        /**
         * @brief Adds all FIX sessions found in a configuration file.
         *
         * Scans the configuration file for groups containing "SESSION"
         * in their name and creates a FIX session for each.
         *
         * @param session_config_file_path Path to the session configuration file.
         *
         * @return true if at least one session was added successfully, false otherwise.
         */
        [[nodiscard]] bool add_sessions_from(const std::string& session_config_file_path)
        {
            Configuration config_file;
            std::string config_load_error;

            if (config_file.load_from_file(session_config_file_path, config_load_error) == false)
            {
                return false;
            }

            std::vector<std::string> session_names;
            config_file.get_group_names(session_names);

            int added_session_count{0};

            for (const auto& session_name : session_names)
            {
                auto lowered_session_name = StringUtilities::to_lower(session_name);

                if (StringUtilities::contains(lowered_session_name, "session"))
                {
                    FixSessionSettings session_settings;

                    if (session_settings.load_from_config_file(session_config_file_path, session_name) == false)
                    {
                        LLFIX_LOG_ERROR("Loading settings for session " + session_name + " failed : " + session_settings.config_load_error);
                        return false;
                    }

                    if (internal_add_session(session_name, session_settings) == false)
                    {
                        return false;
                    }

                    added_session_count++;
                }
            }

            if(added_session_count == 0)
            {
                LLFIX_LOG_ERROR(m_name + " : Could not find any session in " + session_config_file_path + ". Make sure they have 'SESSION' in their names.");
                return false;
            }

            return true;
        }

        /**
         * @brief Returns the number of configured FIX sessions.
         *
         * @return Number of active FIX sessions.
         */
        std::size_t get_session_count() const
        {
            return m_sessions.size();
        }

        bool is_instance_ha_primary() const override
        {
            return m_is_ha_primary;
        }

        /**
         * @brief Retrieves a FIX session by name.
         *
         * @param session_name Name of the FIX session.
         *
         * @return Pointer to the FIX session, or nullptr if not found.
         */
        FixSession* get_session(const std::string& session_name) override
        {
            if (has_session(session_name) == false)
            {
                return nullptr;
            }

            return m_sessions[session_name];
        }

        /**
         * @brief Retrieves the name of a FIX session.
         *
         * @param session Pointer to a FIX session.
         *
         * @return Session name if found, empty string otherwise.
         */
        std::string get_session_name(FixSession* session)
        {
            assert(session);

            for (const auto& session_entry : m_sessions)
            {
                if(session_entry.second == session)
                {
                    return session_entry.first;
                }
            }

            return "";
        }

        /**
         * @brief Retrieves the names of all configured FIX sessions.
         *
         * @param target Output vector populated with session names.
         */
        void get_session_names(std::vector<std::string>& target) override
        {
            for (const auto& iter : m_sessions)
            {
                target.push_back(iter.first);
            }
        }

        std::string get_name() const override
        {
            return m_name;
        }

         /**
         * @brief Specify repeating group definitions for incoming FIX messages for all sessions.
         *
         * @tparam Args Parameter pack defining repeating group structure.
         * @param args Repeating group specification arguments.
         *
         * Not needed in the commercial edition
         */
        template<typename... Args>
        void specify_repeating_group(Args... args)
        {
            FixSession::get_repeating_group_specs().specify_repeating_group(args...);
        }

        #ifdef LLFIX_ENABLE_BINARY_FIELDS
        /**
         * @brief Specify binary field definition for incoming FIX messages.
         *
         * @param message_type FIX Message type
         * @param tag_length FIX tag that will hold binary/raw data size
         * @param tag_data FIX tag that will hold binary/raw data
         *
         * Not needed in the commercial edition
        */
        void specify_binary_field(const std::string& message_type, uint32_t tag_length, uint32_t tag_data)
        {
            FixSession::get_binary_field_specs().specify_binary_field(message_type, tag_length, tag_data);
        }
        #endif

        /**
         * @brief Retrieves the reusable outgoing FIX message instance for a session.
         *
         * @param session Target FIX session.
         *
         * @return Pointer to the outgoing FIX message instance.
         */
        OutgoingFixMessage* outgoing_message_instance(FixSession* session)
        {
            return session->get_outgoing_fix_message();
        }

        /**
         * @brief Encodes and sends an outgoing FIX message.
         *
         * @param session Target FIX session.
         * @param message Outgoing FIX message to send.
         *
         * @return true if the message was sent successfully, false otherwise.
         */
        virtual bool send_outgoing_message(FixSession* session, OutgoingFixMessage* message)
        {
            std::size_t encoded_length = 0;
            auto int_sequence_no = session->get_sequence_store()->get_outgoing_seq_no() + 1;

            message->encode(session->get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, int_sequence_no, encoded_length);
            return send_bytes<true>(session, session->get_tx_encode_buffer(), encoded_length);
        }

        void push_admin_command(const std::string& session_name, ModifyingAdminCommandType type, uint32_t arg = 0) override
        {
            if(llfix_likely(session_name != "*"))
            {
                push_admin_command_internal(session_name, type, arg);
            }
            else
            {
                for (auto& session : m_sessions)
                {
                    push_admin_command_internal(session.first, type, arg);
                }
            }
        }

        #ifdef LLFIX_AUTOMATION
        // Used for testing outgoing msg resends  and gap fills
        bool send_fake_outgoing_message(FixSession* session, OutgoingFixMessage* message)
        {
            std::size_t encoded_length = 0;
            auto int_sequence_no = session->get_sequence_store()->get_outgoing_seq_no() + 1;

            message->encode(session->get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, int_sequence_no, encoded_length);

            auto sequence_store = session->get_sequence_store();
            sequence_store->increment_outgoing_seq_no();

            if(session->serialisation_enabled())
                session->get_outgoing_message_serialiser()->write(reinterpret_cast<const void*>(session->get_tx_encode_buffer()), encoded_length, true, sequence_store->get_outgoing_seq_no());

            session->set_last_sent_message_timestamp_nanoseconds(VDSO::nanoseconds_monotonic());

            return true;
        }

        void set_replay_messages_on_incoming_resend_request_for_all_sessions(bool b)
        {
            for (auto& entry : m_sessions)
            {
                entry.second->set_replay_messages_on_incoming_resend_request(b);
            }
        }
        #endif

        /**
         * @brief Shuts down the FIX server.
         *
         * Stops the underlying transport and terminates all sessions.
         */
        void shutdown() { this->stop(); }
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // TCP connections
        virtual void on_async_io_error(int error_code, int event_result) override
        {
            LLFIX_UNUSED(error_code);
            LLFIX_UNUSED(event_result);
        }

        virtual void on_socket_error(int error_code, int event_result) override
        {
            LLFIX_UNUSED(error_code);
            LLFIX_UNUSED(event_result);
        }
        // Application level messages
        /**
         * @brief Called when a New Order (35=D) message is received.
         *
         * @param session FIX session that received the message.
         * @param message Incoming FIX message.
         */
        virtual void on_new_order(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);};
        /**
         * @brief Called when an Order Cancel Request (35=F) message is received.
         *
         * @param session FIX session that received the message.
         * @param message Incoming FIX message.
         */
        virtual void on_cancel_order(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);};
        /**
         * @brief Called when an Order Cancel/Replace Request (35=G) message is received.
         *
         * @param session FIX session that received the message.
         * @param message Incoming FIX message.
         */
        virtual void on_replace_order(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);};
        /**
         * @brief Called when an application-level reject message is generated.
         *
         * @param session FIX session associated with the reject.
         * @param message Incoming FIX message that caused the reject.
         */
        virtual void on_application_level_reject(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);};
        /**
         * @brief Called when a session-level reject message is generated.
         *
         * @param session FIX session associated with the reject.
         * @param message Incoming FIX message that caused the reject.
         */
        virtual void on_session_level_reject(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);};
        /**
         * @brief Called when any other FIX message type is received.
         *
         * @param session FIX session that received the message.
         * @param message Incoming FIX message.
         */
        virtual void on_custom_message(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);};
        // Session/admin messages
        /**
         * @brief Called when a Logon (35=A) request is received from a client.
         *
         * @param session FIX session requesting logon.
         * @param message Incoming Logon FIX message.
         */
        virtual void on_logon_request(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);}
        /**
         * @brief Called when a Logout (35=5) request is received from a client.
         *
         * @param session FIX session requesting logout.
         * @param message Incoming Logout FIX message.
         */
        virtual void on_logout_request(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);}
        /**
         * @brief Called when a Resend Request (35=2) is received from a client.
         *
         * @param session FIX session that received the resend request.
         * @param message Incoming Resend Request FIX message.
         */
        virtual void on_client_resend_request(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);};
        /**
         * @brief Called when a Test Request (35=1) is received from a client.
         *
         * @param session FIX session that received the test request.
         * @param message Incoming Test Request FIX message.
         */
        virtual void on_client_test_request(FixSession* session, const IncomingFixMessage* message) {LLFIX_UNUSED(session); LLFIX_UNUSED(message);};
        /**
         * @brief Called when a Heartbeat (35=0) message is received from a client.
         *
         * @param session FIX session that received the heartbeat.
         */
        virtual void on_client_heartbeat(FixSession* session) {LLFIX_UNUSED(session);};
        // Others
        /**
         * @brief Applies incoming message throttling logic.
         *
         * Determines whether an incoming FIX message should be processed,
         * delayed, rejected, or cause session termination based on
         * throttling configuration.
         *
         * @param session FIX session receiving the message.
         * @param incoming_fix_message Incoming FIX message.
         *
         * @return true if processing should continue, false otherwise.
         */
        virtual bool process_incoming_throttling(FixSession* session, const IncomingFixMessage* incoming_fix_message)
        {
            if(session->settings()->throttle_limit == 0)
            {
                return true;
            }

            session->throttler()->update();

            if(session->throttler()->reached_limit() )
            {
                session->increment_incoming_throttler_exceed_count();

                if(session->settings()->throttle_action == IncomingThrottlerAction::WAIT)
                {
                    session->throttler()->wait();
                }
                else if(session->settings()->throttle_action == IncomingThrottlerAction::DISCONNECT)
                {
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " : terminating session " + session->get_name() + " as the other end exceeded the throttle rate : " + std::to_string(session->settings()->throttle_limit));
                    this->process_connection_closure(m_fix_connectors.get_peer_index(session));
                    return false;
                }
                else if(session->settings()->throttle_action == IncomingThrottlerAction::REJECT)
                {
                    send_throttle_reject_message(session, incoming_fix_message);
                    return false;
                }
            }

            return true;
        }
        /**
         * @brief Validates the session against configured schedule rules.
         *
         * Checks whether the current time is within the session's allowed
         * schedule and terminates the connection if it is not.
         *
         * @param session FIX session to validate.
         */
        virtual void process_schedule_validator(FixSession* session)
        {
            if (session->is_now_valid_session_datetime() == false)
            {
                LLFIX_LOG_DEBUG("FixServer " + m_name + " : terminating session " + session->get_name() + " due to schedule settings");
                this->process_connection_closure(m_fix_connectors.get_peer_index(session), true);
            }
        }

        /**
         * @brief Accepts and validates an incoming session before logon processing.
         *
         * Handles parser-level validation failures, checks required header tags,
         * resolves the target session from CompIDs and BeginString, and rejects
         * invalid or duplicate live logon attempts.
         *
         * @param peer_index Transport peer index associated with the connection.
         * @param incoming_fix_message Parsed incoming FIX message.
         * @param buffer Raw incoming FIX bytes for logging/reject context.
         * @param buffer_length Length of the raw incoming FIX buffer.
         * @param parser_reject_code Parser validation result code.
         *
         * @return Pointer to the resolved FIX session, or nullptr if rejected.
         */
        virtual FixSession* accept_session(std::size_t peer_index, const IncomingFixMessage* incoming_fix_message, const char* buffer, std::size_t buffer_length, uint32_t parser_reject_code)
        {
            if (parser_reject_code != static_cast<uint32_t>(-1))
            {
                std::string message;
                if (parser_reject_code > FixConstants::FIX_MAX_ERROR_CODE)
                {
                    message = FixParserErrorCodes::get_internal_parser_error_description(parser_reject_code) + " : " + FixUtilities::fix_to_human_readible(buffer, buffer_length);
                }
                else
                {
                    char temp_buf[512];
                    std::size_t length{ 0 };
                    FixUtilities::get_reject_reason_text(temp_buf, length, parser_reject_code);
                    message = temp_buf;
                    message += " , message : " + FixUtilities::fix_to_human_readible(buffer, buffer_length);
                }
                process_invalid_logon(peer_index, message);
                return nullptr;
            }

            FixSession* session = nullptr;

            if (!(incoming_fix_message->has_tag(FixConstants::TAG_BEGIN_STRING) && incoming_fix_message->has_tag(FixConstants::TAG_MSG_SEQ_NUM) && incoming_fix_message->has_tag(FixConstants::TAG_SENDER_COMP_ID) && incoming_fix_message->has_tag(FixConstants::TAG_TARGET_COMP_ID) && incoming_fix_message->has_tag(FixConstants::TAG_MSG_TYPE)))
            {
                process_invalid_logon(peer_index, "One of following required tags is missing : 8,34,49,56,35 , message : " + FixUtilities::fix_to_human_readible(buffer, buffer_length));
                return nullptr;
            }
            else
            {
                auto message_type = incoming_fix_message->get_tag_value_as<char>(FixConstants::TAG_MSG_TYPE);

                if (message_type == FixConstants::MSG_TYPE_LOGON)
                {
                    auto incoming_begin_string = incoming_fix_message->get_tag_value_as<std::string>(FixConstants::TAG_BEGIN_STRING);
                    auto incoming_comp_id = incoming_fix_message->get_tag_value_as<std::string>(FixConstants::TAG_SENDER_COMP_ID);
                    auto incoming_target_comp_id = incoming_fix_message->get_tag_value_as<std::string>(FixConstants::TAG_TARGET_COMP_ID);

                    session = find_session(incoming_begin_string, incoming_target_comp_id, incoming_comp_id);

                    if (session != nullptr)
                    {
                        auto session_state = session->get_state();

                        if (is_state_live(session_state)) // This check is the most important part of multithreaded FixServer
                        {
                            std::string message = "Incoming logon message for server " + m_name + " is invalid as the client is already logged on, session : " + session->get_name();
                            process_invalid_logon(peer_index, message);
                            return nullptr;
                        }

                        if(session_state == SessionState::DISABLED)
                        {
                            std::string message = "Cannot accept connection attempt as session " + session->get_name() + " is disabled. Closing connection.";
                            process_invalid_logon(peer_index, message);
                            return nullptr;
                        }

                        if (session->is_now_valid_session_datetime() == false)
                        {
                            std::string message = "Cannot accept connection attempt for session " + session->get_name() + " due to schedule settings. Closing connection.";
                            process_invalid_logon(peer_index, message);
                            return nullptr;
                        }

                        //////////////////////////////////////////////////////////////////////////////////
                        m_fix_connectors.update(peer_index, session);

                        if constexpr(Transport::is_multithreaded())
                        {
                            this->mark_connector_as_ready_for_worker_thread_dispatch(peer_index);
                        }
                        //////////////////////////////////////////////////////////////////////////////////
                        session->reset_flags();
                        session->get_incoming_fix_message()->reset();
                        session->get_incoming_fix_message()->copy_non_dirty_tag_values_from(*incoming_fix_message);

                        if(incoming_fix_message->has_tag(FixConstants::TAG_RESET_SEQ_NUM_FLAG))
                        {
                            if(incoming_fix_message->get_tag_value_as<char>(FixConstants::TAG_RESET_SEQ_NUM_FLAG) == FixConstants::FIX_BOOLEAN_TRUE)
                            {
                                session->get_sequence_store()->reset_numbers();
                            }
                        }

                        session->set_state(SessionState::PENDING_LOGON);
                        return session;
                    }
                    else
                    {
                        std::string message = "Could not find a session for incoming message : 8=" + incoming_begin_string + " 49=" + incoming_comp_id + " 56=" + incoming_target_comp_id + " , message : " + FixUtilities::fix_to_human_readible(buffer, buffer_length);
                        process_invalid_logon(peer_index, message);
                        return nullptr;
                    }
                }
                else
                {
                    std::string message = std::string("Received a message with a msgtype different than A (") + message_type + ") while expecting a logon message : " + FixUtilities::fix_to_human_readible(buffer, buffer_length);
                    process_invalid_logon(peer_index, message);
                    return nullptr;
                }
            }
        }

        /**
         * @brief Processes an incoming Logon (35=A) request.
         *
         * Validates and authenticates the incoming logon, applies heartbeat
         * timing from the request, sends the logon response, and finalises
         * session state transition to logged on.
         *
         * @param session FIX session handling the logon request.
         * @param peer_index Transport peer index associated with the connection.
         * @param message Incoming Logon FIX message.
         */
        virtual void process_logon_request(FixSession* session, std::size_t peer_index, const IncomingFixMessage* message)
        {
            if (validate_logon_request(session, peer_index, message) == false)
            {
                process_invalid_logon(peer_index, "");
                return;
            }

            if (authenticate_logon_request(session, message) == false)
            {
                std::string message = "Authentication failed for incoming logon message for server " + m_name + " for session " + session->get_name();
                process_invalid_logon(peer_index, message);
                return;
            }

            auto requested_heartbeat_interval = message->get_tag_value_as<uint32_t>(FixConstants::TAG_HEART_BT_INT);
            auto requested_heartbeat_interval_nanoseconds = static_cast<uint64_t>(requested_heartbeat_interval) * static_cast<uint64_t>(1'000'000'000);

            session->set_heartbeart_interval_in_nanoseconds(requested_heartbeat_interval_nanoseconds);
            session->set_outgoing_test_request_interval_in_nanoseconds(static_cast<uint64_t>(requested_heartbeat_interval_nanoseconds * session->settings()->outgoing_test_request_interval_multiplier));

            send_logon_response(session, message);
            session->set_state(SessionState::LOGGED_ON);

            LLFIX_LOG_DEBUG(m_name + ", session logged on : " + session->get_name() + " , peer index : " + std::to_string(peer_index));

            do_post_logon_sequence_number_check(session);

            on_logon_request(session, message);
        }

        /**
         * @brief Authenticates an incoming Logon (35=A) request.
         *
         * Allows applications to implement custom authentication logic
         * such as logon usernames and password
         *
         * @param session FIX session attempting to log on.
         * @param message Incoming Logon FIX message.
         *
         * @return true if authentication succeeds, false to reject the logon.
         */
        virtual bool authenticate_logon_request(FixSession* session, const IncomingFixMessage* message)
        {
            LLFIX_UNUSED(session);
            LLFIX_UNUSED(message);
            return true;
        }
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    public:
        void on_data_ready(std::size_t peer_index) override
        {
            auto read = this->receive(peer_index);

            if (read > 0 && read <= static_cast<int>(this->m_options.m_rx_buffer_capacity))
            {
                auto buffer_size = this->get_rx_buffer_size(peer_index);
                if(buffer_size >0 && buffer_size != static_cast<std::size_t>(-1))
                    process_rx_buffer(peer_index, this->get_rx_buffer(peer_index), buffer_size);
            }

            this->receive_done(peer_index);
        }

        void on_client_connected(std::size_t peer_index) override
        {
            LLFIX_LOG_DEBUG("FixServer " + m_name + " : new client connected , peer index : " + std::to_string(peer_index));
        }

        void on_client_disconnected(std::size_t peer_index) override
        {
            Transport::on_client_disconnected(peer_index);

            if (m_fix_connectors.has_peer(peer_index) == false)
                return;

            // A logged on client
            auto session = m_fix_connectors.get_session(peer_index);

            if(session != nullptr)
            {
                LLFIX_LOG_DEBUG("FixServer " + m_name + " : session " + session->get_name() + " disconnected , peer index : " + std::to_string(peer_index));
                on_client_disconnected(session);
            }

            // Remove entries
            m_fix_connectors.remove(peer_index, session);
        }

        void on_client_disconnected(FixSession* session)
        {
            session->set_state(SessionState::DISCONNECTED);
        }

        FixServerSettings& get_settings() { return m_settings; }

        std::string get_settings_as_string(const std::string& delimiter) override
        {
            return m_settings.to_string(delimiter);
        }

        /**
         * @brief Set the message persistence plugin.
         *
         * @param plugin Pointer to a MessagePersistPlugin implementation.
         */
        void set_message_persist_plugin(MessagePersistPlugin* plugin)
        {
            assert(plugin);
            m_message_persist_plugin = plugin;
        }

    protected:
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // ADMIN
        /**
         * @brief Sends a Heartbeat (35=0) message to the client.
         *
         * @param session FIX session to send the heartbeat on.
         * @param test_request_id Optional TestReqID (tag 112), may be null.
         *
         * @return true if the message was sent successfully, false otherwise.
         */
        virtual bool send_heartbeat(FixSession* session, FixString* test_request_id)
        {
            session->build_heartbeat_message(outgoing_message_instance(session), test_request_id);
            return send_outgoing_message(session, outgoing_message_instance(session));
        }

        /**
         * @brief Sends a Logon (35=A) response message.
         *
         * @param session FIX session to send the logon response on.
         * @param message Incoming Logon FIX message being responded to.
         *
         * @return true if the message was sent successfully, false otherwise.
         */
        virtual bool send_logon_response(FixSession* session, const IncomingFixMessage* message)
        {
            LLFIX_UNUSED(message);
            auto response = outgoing_message_instance(session);
            response->set_msg_type(FixConstants::MSG_TYPE_LOGON);

            // TAG 98 ENCRYPTION METHOD
            response->set_tag(FixConstants::TAG_ENCRYPT_METHOD, 0);

            // TAG 108 HEARTBEAT INTERVAL
            response->set_tag(FixConstants::TAG_HEART_BT_INT, static_cast<uint32_t>(session->get_heartbeart_interval_in_nanoseconds() / 1'000'000'000));

            // TAG 1137 DEFAULT APP VER ID
            auto default_app_ver_id = session->get_default_app_ver_id();

            if (default_app_ver_id.length() > 0)
            {
                response->set_tag(FixConstants::TAG_DEFAULT_APPL_VER_ID, default_app_ver_id);
            }

            return send_outgoing_message(session, response);
        }

        /**
         * @brief Sends a Logout (35=5) message.
         *
         * @param session FIX session to send the logout message on.
         * @param reason_text Optional human-readable logout reason.
         *
         * @return true if the message was sent successfully, false otherwise.
         */
        virtual bool send_logout_message(FixSession* session, const std::string& reason_text = "")
        {
            session->build_logout_message(outgoing_message_instance(session), reason_text);
            return send_outgoing_message(session, outgoing_message_instance(session));
        }

        /**
         * @brief Sends a Test Request (35=1) message.
         *
         * @param session FIX session to send the test request on.
         *
         * @return true if the message was sent successfully, false otherwise.
         */
        virtual bool send_test_request(FixSession* session)
        {
            session->build_test_request_message(outgoing_message_instance(session));
            return send_outgoing_message(session, outgoing_message_instance(session));
        }

        /**
         * @brief Sends a Resend Request (35=2) message.
         *
         * @param session FIX session to send the resend request on.
         *
         * @return true if the message was sent successfully, false otherwise.
         */
        virtual bool send_resend_request(FixSession* session)
        {
            session->build_resend_request_message(outgoing_message_instance(session), "0");
            return send_outgoing_message(session, outgoing_message_instance(session));
        }

        /**
         * @brief Sends a Sequence Reset (35=4) message without gap fill.
         *
         * @param session FIX session to send the sequence reset on.
         * @param desired_sequence_no Target sequence number.
         *
         * @return true if the message was sent successfully, false otherwise.
         */
        virtual bool send_sequence_reset_message(FixSession* session, uint32_t desired_sequence_no)
        {
            /*
                This message is a "hard" gap fill that doesn't respond to an incoming 35=2/resend request but to an internal admin request
                Therefore we can't set 123=Y
            */
            auto message = outgoing_message_instance(session);
            session->build_sequence_reset_message(message, desired_sequence_no);

            // ENCODE
            std::size_t encoded_length = 0;
            message->encode(session->get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, desired_sequence_no, encoded_length);

            // SEND
            return send_bytes<false>(session, session->get_tx_encode_buffer(), encoded_length); // false is for not incrementing seq no for this message
        }

        /**
         * @brief Sends a Gap Fill message in response to a Resend Request.
         *
         * @param session FIX session to send the gap fill on.
         *
         * @return true if the message was sent successfully, false otherwise.
         */
        virtual bool send_gap_fill_message(FixSession* session)
        {
            /*
                This message is a gap fill that responds to an incoming 35=2/resend request.
                For "hard" gap fills that don't set 123=Y, see send_sequence_reset_message
            */
            auto message = outgoing_message_instance(session);
            session->build_gap_fill_message(message);

            // ENCODE
            std::size_t encoded_length = 0;
            message->encode(session->get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, session->get_incoming_resend_request_begin_no(), encoded_length); // We need to encode with seq no that the peer expects hence using m_incoming_resend_request_begin_no

            // SEND
            return send_bytes<false>(session, session->get_tx_encode_buffer(), encoded_length); // false is for not incrementing seq no for this message
        }

        virtual void resend_messages_to_client(FixSession* session, uint32_t begining_seq_no, uint32_t end_seq_no)
        {
            for (uint32_t i = begining_seq_no; i <= end_seq_no; i++)
            {
                std::size_t message_length = 0;
                session->get_outgoing_message_serialiser()->read_message(i, session->get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, message_length);

                auto message = outgoing_message_instance(session);
                message->load_from_buffer(session->get_tx_encode_buffer(), message_length); // t122(orig sending time) will be handled by load_from_buffer

                message->template set_tag<FixMessageComponent::HEADER>(FixConstants::TAG_POSS_DUP_FLAG, FixConstants::FIX_BOOLEAN_TRUE);

                if (session->include_t97_during_resends())
                {
                    message->template set_tag<FixMessageComponent::HEADER>(FixConstants::TAG_POSS_RESEND, FixConstants::FIX_BOOLEAN_TRUE);
                }

                std::size_t encoded_length = 0;
                message->encode(session->get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, i, encoded_length);
                send_bytes<false>(session, session->get_tx_encode_buffer(), encoded_length); // false is for not incrementing seq no for this message
            }
        }

        virtual bool send_reject_message(FixSession* session, const IncomingFixMessage* incoming_message, uint32_t reject_reason_code, const char* buffer_message, std::size_t buffer_message_length, uint32_t error_tag=0)
        {
            LLFIX_UNUSED(incoming_message);
            char reject_reason_text[256];

            std::size_t text_length{ 0 };
            FixUtilities::get_reject_reason_text(reject_reason_text, text_length, reject_reason_code);

            if (error_tag == 0)
            {
                LLFIX_LOG_DEBUG("FixServer " + m_name + " session " + session->get_name() + " received an invalid message : " + std::string(reject_reason_text) + " ,message : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
            }
            else
            {
                LLFIX_LOG_DEBUG("FixServer " + m_name + " session " + session->get_name() + " received an invalid message : " + std::string(reject_reason_text) + " ,tag : " + std::to_string(error_tag) + " ,message : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
            }

            session->build_session_level_reject_message(outgoing_message_instance(session), reject_reason_code, reject_reason_text, error_tag);
            return send_outgoing_message(session, outgoing_message_instance(session));
        }

        virtual bool send_throttle_reject_message(FixSession* session, const IncomingFixMessage* incoming_message)
        {
            auto reject_message = outgoing_message_instance(session);
            reject_message->set_msg_type(FixConstants::MSG_TYPE_BUSINESS_REJECT);

            reject_message->set_tag(FixConstants::TAG_TEXT, session->settings()->throttler_reject_message);

            if (incoming_message->has_tag(FixConstants::TAG_MSG_SEQ_NUM))
            {
                reject_message->set_tag(FixConstants::TAG_REF_SEQ_NUM, incoming_message->get_tag_value_as<uint32_t>(FixConstants::TAG_MSG_SEQ_NUM));
            }

            if (incoming_message->has_tag(FixConstants::TAG_MSG_TYPE))
            {
                reject_message->set_tag(FixConstants::TAG_REF_MSG_TYPE, incoming_message->get_tag_value_as<std::string>(FixConstants::TAG_MSG_TYPE));
            }

            return send_outgoing_message(session, reject_message);
        }

        std::unordered_map<std::string, FixSession*> m_sessions;

    private:
        static inline constexpr std::size_t MINIMUM_REQUIRED_INITIAL_BUFFER_FOR_PARSING = 4; // 10=<DELIMITER>
        std::string m_name;
        bool m_is_ha_primary = true;

        FixServerSettings m_settings;

        FixServerConnectors m_fix_connectors;

        MessagePersistPlugin* m_message_persist_plugin = nullptr;

        UserspaceSpinlock<> m_unknown_session_context_lock;

        bool has_session(const std::string& session_name)
        {
            if (m_sessions.find(session_name) == m_sessions.end())
            {
                return false;
            }

            return true;
        }

        FixSession* find_session(const std::string& begin_string, const std::string& sender_comp_id, const std::string& target_comp_id)
        {
            FixSession* ret = nullptr;

            for (const auto& session_entry : m_sessions)
            {
                if (session_entry.second->settings()->sender_comp_id == sender_comp_id && session_entry.second->settings()->target_comp_id == target_comp_id && session_entry.second->settings()->begin_string == begin_string)
                {
                    return session_entry.second;
                }
            }

            return ret;
        }

        bool does_any_session_use_path(const std::string& path)
        {
            for (const auto& session_entry : m_sessions)
            {
                if (session_entry.second->settings()->sequence_store_file_path == path)
                {
                    return true;
                }

                if (session_entry.second->settings()->incoming_message_serialisation_path == path)
                {
                    return true;
                }

                if (session_entry.second->settings()->outgoing_message_serialisation_path == path)
                {
                    return true;
                }
            }
            return false;
        }

        bool does_any_session_use_compids(const std::string& sender_comp_id, const std::string& target_comp_id)
        {
            for (const auto& session_entry : m_sessions)
            {
                if (session_entry.second->settings()->sender_comp_id == sender_comp_id && session_entry.second->settings()->target_comp_id == target_comp_id)
                {
                    return true;
                }
            }
            return false;
        }

        bool internal_add_session(const std::string& session_name, const FixSessionSettings& session_settings)
        {
            if (has_session(session_name))
            {
                LLFIX_LOG_ERROR(m_name + " : Already loaded a session with name " + session_name + " . You have to specify unique names for sessions.");
                return false;
            }

            if (does_any_session_use_path(session_settings.sequence_store_file_path))
            {
                LLFIX_LOG_ERROR(m_name + " : session " + session_name + "'s sequence_store_file_path already being used in another session.");
                return false;
            }

            if (does_any_session_use_path(session_settings.incoming_message_serialisation_path))
            {
                LLFIX_LOG_ERROR(m_name + " : session " + session_name + "'s incoming_message_serialisation_path already being used in another session.");
                return false;
            }

            if (does_any_session_use_path(session_settings.outgoing_message_serialisation_path))
            {
                LLFIX_LOG_ERROR(m_name + " : session " + session_name + "'s outgoing_message_serialisation_path already being used in another session.");
                return false;
            }

            if (does_any_session_use_compids(session_settings.sender_comp_id, session_settings.target_comp_id))
            {
                std::string warning_message = "WARNING (" + m_name + "): session " + session_name + "'s sender and target comp ids are already being used in another session.";
                LLFIX_LOG_WARNING(warning_message);
                fprintf(stderr, "%s", warning_message.c_str());
            }

            FixSession* current_session = new (std::nothrow) FixSession;

            if (current_session == nullptr)
            {
                LLFIX_LOG_ERROR(m_name + " : Failed to allocate fix session for " + session_name);
                return false;
            }

            session_settings.is_server = true;
            session_settings.tx_encode_buffer_capacity = m_settings.tx_encode_buffer_capacity; // Propagate

            if (current_session->initialise(session_name, session_settings) == false)
            {
                delete current_session;
                return false;
            }

            m_sessions[session_name] = current_session;

            if (m_settings.starts_as_primary_instance == false)
            {
                disable_session(current_session);
            }

            return true;
        }

        void process_session(std::size_t peer_index)
        {
            static_assert(Transport::is_multithreaded());

            FixSession* sess = m_fix_connectors.get_session(peer_index);

            if (sess)
            {
                sess->lock();
                process_session(sess);
                sess->unlock();
            }
        }

        void process_session(FixSession* session)
        {
            static_assert(Transport::is_multithreaded());

            if (llfix_likely(this->m_is_stopping.load() == false))
            {
                process_admin_commands(session);

                auto current_state = session->get_state();

                if (is_state_live(current_state))
                {
                    process_outgoing_resend_request_if_necessary(session, VDSO::nanoseconds_monotonic());
                    process_outgoing_test_request_if_necessary(session, VDSO::nanoseconds_monotonic());
                    respond_to_resend_request_if_necessary(session);
                    respond_to_test_request_if_necessary(session);
                    send_heartbeat_if_necessary(session, VDSO::nanoseconds_monotonic());

                    process_schedule_validator(session);
                }
            }
            else
            {
                send_logout_message(session);
                on_client_disconnected(session);
            }
        }

        void process_admin_commands_of_all_non_live_sessions()
        {
            static_assert(Transport::is_multithreaded());

            for (auto& iter : m_sessions)
            {
                auto state = iter.second->get_state();

                if (is_state_live(state) == false)
                {
                    iter.second->lock();
                    process_admin_commands(iter.second);
                    iter.second->unlock();
                }
            }

        }

        void process()
        {
            static_assert(!Transport::is_multithreaded());

            if (llfix_likely(this->m_is_stopping.load() == false))
            {
                for (const auto& session_entry : m_sessions)
                {
                    FixSession* current_session = session_entry.second;

                    process_admin_commands(current_session);

                    auto current_state = current_session->get_state();

                    if (is_state_live(current_state))
                    {
                        process_outgoing_resend_request_if_necessary(current_session, VDSO::nanoseconds_monotonic());
                        process_outgoing_test_request_if_necessary(current_session, VDSO::nanoseconds_monotonic());
                        respond_to_resend_request_if_necessary(current_session);
                        respond_to_test_request_if_necessary(current_session);
                        send_heartbeat_if_necessary(current_session, VDSO::nanoseconds_monotonic());

                        process_schedule_validator(current_session);
                    }
                }
            }
            else
            {
                send_logout_messages_to_all_sessions();
            }
        }

        void push_admin_command_internal(const std::string& session_name, ModifyingAdminCommandType type, uint32_t arg = 0)
        {
            auto admin_command = new (std::nothrow) ModifyingAdminCommand;

            if (llfix_likely(admin_command))
            {
                admin_command->type = type;
                admin_command->arg = arg;
                m_sessions[session_name]->get_admin_commands()->push(admin_command); // Session name existence is checked in management server layer
            }
            else
            {
                LLFIX_LOG_ERROR("Failed to process setter admin command for FixServer " + m_name);
            }
        }

        void process_admin_commands(FixSession* session)
        {
            ModifyingAdminCommand* admin_command{ nullptr };

            if (session->get_admin_commands()->try_pop(&admin_command) == true)
            {
                switch (admin_command->type)
                {
                    case ModifyingAdminCommandType::SET_INCOMING_SEQUENCE_NUMBER:
                    {
                        session->get_sequence_store()->set_incoming_seq_no(admin_command->arg);
                        LLFIX_LOG_DEBUG(m_name + " : processed set incoming sequence number admin command , session name : " + session->get_name() + " , new seq no : " + std::to_string(admin_command->arg) );
                        break;
                    }

                    case ModifyingAdminCommandType::SET_OUTGOING_SEQUENCE_NUMBER:
                    {
                        session->get_sequence_store()->set_outgoing_seq_no(admin_command->arg);
                        LLFIX_LOG_DEBUG(m_name + " : processed set outgoing sequence number admin command , session name : " + session->get_name() + " , new seq no : " + std::to_string(admin_command->arg) );
                        break;
                    }

                    case ModifyingAdminCommandType::SEND_SEQUENCE_RESET:
                    {
                        if( session->get_state() == SessionState::LOGGED_ON )
                        {
                            send_sequence_reset_message(session, admin_command->arg);
                            LLFIX_LOG_DEBUG(m_name + " : processed send sequence reset admin command , session name : " + session->get_name() + " , new seq no : " + std::to_string(admin_command->arg) );
                        }
                        else
                        {
                            LLFIX_LOG_ERROR(m_name + " : dropping send sequence reset admin command as not logged on , session name : " + session->get_name());
                        }

                        break;
                    }

                    case ModifyingAdminCommandType::DISABLE_SESSION:
                    {
                        disable_session(session);
                        LLFIX_LOG_DEBUG(m_name + " : processed disable session admin command for session " + session->get_name() + " , new session state : Disabled");
                        break;
                    }

                    case ModifyingAdminCommandType::ENABLE_SESSION:
                    {
                        if(m_is_ha_primary == true)
                        {
                            enable_session(session);
                            LLFIX_LOG_DEBUG(m_name + " : processed enable session admin command for " + session->get_name());
                        }
                        else
                        {
                            LLFIX_LOG_ERROR(m_name + " ignoring enable session admin command for " + session->get_name() + " since not a primary instance" );
                        }
                        break;
                    }

                    case ModifyingAdminCommandType::SET_IS_HA_PRIMARY_INSTANCE:
                    {
                        m_is_ha_primary = admin_command->arg == 1 ? true : false;

                        if(admin_command->arg == 1)
                        {
                            enable_session(session);
                            LLFIX_LOG_DEBUG(m_name + " : processed set is ha primary instance 1 admin command for " + session->get_name());
                        }
                        else
                        {
                            disable_session(session);
                            LLFIX_LOG_DEBUG(m_name + " : processed set is ha primary instance 0 admin command for " + session->get_name());
                        }

                        break;
                    }


                    default: break;
                }

                delete admin_command;
            }
        }

        void disable_session(FixSession* session)
        {
            if (is_state_live(session->get_state()))
            {
                this->process_connection_closure(m_fix_connectors.get_peer_index(session), true);
            }
            session->set_state(SessionState::DISABLED);
        }

        void enable_session(FixSession* session)
        {
            if(session->get_state() == SessionState::DISABLED)
            {
                session->set_state(SessionState::DISCONNECTED);
                if(m_settings.refresh_resend_cache_during_promotion)
                    session->reinitialise_outgoing_serialiser();
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // ADMIN UTILITY
        void process_outgoing_resend_request_if_necessary(FixSession* session, uint64_t current_timestamp_nanoseconds)
        {
            if (session->get_state() == SessionState::IN_RETRANSMISSION_INITIATED_BY_SELF)
            {
                // TERMINATE SESSION IF NECESSARY
                uint64_t delta_nanoseconds = current_timestamp_nanoseconds - session->outgoing_resend_request_timestamp_nanoseconds();
                uint64_t required_delta_nanoseconds = static_cast<uint64_t>(session->outgoing_resend_request_expire_secs()) * 1'000'000'000;

                if (delta_nanoseconds >= required_delta_nanoseconds)
                {
                    // We need to terminate the session as the other side didn't respond properly to our outgoing resend request
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " : terminating session " + session->get_name() + " as the other end didn't respond to 35=2/resend request in pre-configured timeout (outgoing_resend_request_expire_secs) : " + std::to_string(session->outgoing_resend_request_expire_secs()));
                    this->process_connection_closure(m_fix_connectors.get_peer_index(session));
                }
            }
            else
            {
                // SEND IF NECESSARY
                if (session->needs_to_send_resend_request())
                {
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " session " + session->get_name() + " peer " + std::to_string(m_fix_connectors.get_peer_index(session)) + " : sending resend request , begin no : " + std::to_string(session->get_outgoing_resend_request_begin_no()));
                    send_resend_request(session);
                    session->set_outgoing_resend_request_timestamp_nanoseconds(current_timestamp_nanoseconds);
                    session->set_state(llfix::SessionState::IN_RETRANSMISSION_INITIATED_BY_SELF);
                    session->set_needs_to_send_resend_request(false);
                }
            }
        }

        void process_outgoing_test_request_if_necessary(FixSession* session, uint64_t current_timestamp_nanoseconds)
        {
            if (session->expecting_response_for_outgoing_test_request() == false)
            {
                // SEND IF NECESSARY
                auto delta_nanoseconds = current_timestamp_nanoseconds - session->last_received_message_timestamp_nanoseconds();

                if (delta_nanoseconds >= (session->get_outgoing_test_request_interval_in_nanoseconds()))
                {
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " session " + session->get_name() + " : sending test request");
                    send_test_request(session);
                    session->set_outgoing_test_request_timestamp_nanoseconds(current_timestamp_nanoseconds);
                    session->set_expecting_response_for_outgoing_test_request(true);
                }
            }
            else
            {
                // TERMINATE SESSION IF NECESSARY
                auto delta_nanoseconds = current_timestamp_nanoseconds - session->outgoing_test_request_timestamp_nanoseconds();

                if (delta_nanoseconds >= (session->get_heartbeart_interval_in_nanoseconds() * static_cast<uint64_t>(2)))
                {
                    session->set_expecting_response_for_outgoing_test_request(false);
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " : terminating session " + session->get_name() + " as the other end didn't respond to 35=1/test request");
                    this->process_connection_closure(m_fix_connectors.get_peer_index(session));
                }
            }
        }

        void respond_to_resend_request_if_necessary(FixSession* session)
        {
            if (session->needs_responding_to_incoming_resend_request())
            {
                const auto last_outgoing_seq_no = session->get_sequence_store()->get_outgoing_seq_no();

                // Partial replays not supported therefore incoming end no should be either 0 or same as last outgoing seq no
                bool will_replay = (session->serialisation_enabled()) && (session->replay_messages_on_incoming_resend_request() && (session->get_incoming_resend_request_end_no() == 0
                                   || session->get_incoming_resend_request_end_no() == last_outgoing_seq_no) && session->get_incoming_resend_request_begin_no()<=last_outgoing_seq_no);

                if(last_outgoing_seq_no > session->get_incoming_resend_request_begin_no())
                    if(last_outgoing_seq_no-session->get_incoming_resend_request_begin_no()+1 > session->max_resend_range())
                        will_replay = false;

                if(will_replay)
                {
                    for (uint32_t i = session->get_incoming_resend_request_begin_no(); i <= last_outgoing_seq_no; i++)
                    {
                        if (session->get_outgoing_message_serialiser()->has_message_in_memory(i) == false)
                        {
                            will_replay = false;
                            break;
                        }
                    }
                }

                if(will_replay)
                {
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " session " + session->get_name() + " : replaying messages , begin seq no : " + std::to_string(session->get_incoming_resend_request_begin_no()) + " , end seq no : " + std::to_string(last_outgoing_seq_no));
                    resend_messages_to_client(session, session->get_incoming_resend_request_begin_no(), last_outgoing_seq_no);
                }
                else
                {
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " session " + session->get_name() + " : sending gap fill message");
                    send_gap_fill_message(session);
                }

                session->set_needs_responding_to_incoming_resend_request(false);
                session->set_state(llfix::SessionState::LOGGED_ON);
            }
        }

        void respond_to_test_request_if_necessary(FixSession* session)
        {
            if (session->needs_responding_to_incoming_test_request())
            {
                if (session->get_incoming_test_request_id()->length() > 0)
                {
                    send_heartbeat(session , session->get_incoming_test_request_id());
                    session->get_incoming_test_request_id()->set_length(0);
                }
                else
                {
                    // We should not hit here , but better than risking disconnection
                    send_heartbeat(session, nullptr);
                }

                LLFIX_LOG_DEBUG("FixServer " + m_name + " session " + session->get_name() + " : responded to incoming test request");
                session->set_needs_responding_to_incoming_test_request(false);
            }
        }

        void send_heartbeat_if_necessary(FixSession* session, uint64_t current_timestamp_nanoseconds)
        {
            auto delta_nanoseconds = current_timestamp_nanoseconds - session->last_sent_message_timestamp_nanoseconds();

            if (delta_nanoseconds >= (session->get_heartbeart_interval_in_nanoseconds() * static_cast<uint64_t>(9) / static_cast<uint64_t>(10))) // 0.9 for safety
            {
                send_heartbeat(session, nullptr);
            }
        }

        void send_logout_messages_to_all_sessions()
        {
            for (const auto& session_entry : m_sessions)
            {
                FixSession* current_session = session_entry.second;

                if (current_session->get_state() != SessionState::DISCONNECTED)
                {
                    send_logout_message(current_session);
                    on_client_disconnected(current_session);
                }
            }
        }
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // TO CLIENT
        template <bool increment_outgoing_seq_no>
        LLFIX_FORCE_INLINE bool send_bytes(FixSession* session, const char* buffer, std::size_t buffer_size)
        {
            std::size_t peer_index = m_fix_connectors.get_peer_index(session);

            if (peer_index == static_cast<std::size_t>(-1))
                return false;

            auto sequence_store = session->get_sequence_store();

            auto ret = this->send(peer_index, buffer, buffer_size);

            if constexpr (increment_outgoing_seq_no == true)
            {
                if(ret==true)
                    sequence_store->increment_outgoing_seq_no();
            }

            if(session->serialisation_enabled())
                session->get_outgoing_message_serialiser()->write(reinterpret_cast<const void*>(buffer), buffer_size, ret, sequence_store->get_outgoing_seq_no());

            if (m_message_persist_plugin)
                m_message_persist_plugin->persist_outgoing_message(session->get_name(), sequence_store->get_outgoing_seq_no(), buffer, buffer_size, ret);

            session->set_last_sent_message_timestamp_nanoseconds(VDSO::nanoseconds_monotonic());

            return ret;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // THREAD LOCAL UNKNOWN SESSION CONTEXT
        UnknownSessionContext* get_thread_local_unknown_session_context()
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_unknown_session_context_lock);

            UnknownSessionContext* unknown_session_context = nullptr;
            unknown_session_context = reinterpret_cast<UnknownSessionContext*>(ThreadLocalStorage::get_instance().get());

            if (unknown_session_context == nullptr)
            {
                unknown_session_context = new(std::nothrow) UnknownSessionContext;
                ThreadLocalStorage::get_instance().set(unknown_session_context);
            }

            if (unknown_session_context)
            {
                if (unknown_session_context->m_incoming_fix_message.initialise() == false)
                {
                    LLFIX_LOG_ERROR(m_name + " : Failed to initialise unknown session context's incoming_fix_message");
                    return nullptr;
                }

                if (unknown_session_context->m_fix_string_view_cache.create(256) == false)
                {
                    LLFIX_LOG_ERROR(m_name + " : Failed to create  unknown session context's fix string view cache");
                    return nullptr;
                }
            }

            return unknown_session_context;
        }

        void destroy_thread_local_unknown_session_context()
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_unknown_session_context_lock);

            UnknownSessionContext* unknown_session_context{ nullptr };
            unknown_session_context = reinterpret_cast<UnknownSessionContext*>(ThreadLocalStorage::get_instance().get());

            if (unknown_session_context)
            {
                delete unknown_session_context;
                ThreadLocalStorage::get_instance().set(nullptr);
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // FROM CLIENT
        LLFIX_HOT void process_rx_buffer(std::size_t peer_index, char* buffer, std::size_t buffer_size)
        {
            std::size_t buffer_read_index = 0;

            if (llfix_unlikely(buffer_size < MINIMUM_REQUIRED_INITIAL_BUFFER_FOR_PARSING))
            {
                this->set_incomplete_buffer(peer_index, buffer, buffer_size);
                return;
            }

            FixSession* session = m_fix_connectors.get_session(peer_index);

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // FIND OUT THE END
            int final_tag10_delimiter_index{ -1 };
            int current_index = static_cast<int>(buffer_size - 1);

            if(llfix_unlikely(FixUtilities::find_delimiter_from_end(buffer, buffer_size, current_index) == false))
            {
                // We don't have the entire message
                this->set_incomplete_buffer(peer_index, buffer, buffer_size);
                return;
            }

            FixUtilities::find_tag10_start_from_end(buffer, buffer_size, current_index, final_tag10_delimiter_index);

            if (final_tag10_delimiter_index == -1)
            {
                // We don't have the entire message
                this->set_incomplete_buffer(peer_index, buffer, buffer_size);
                return;
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // AT THIS POINT WE HAVE AT LEAST ONE COMPLETE MESSAGE
            while (true)
            {
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // FIND OUT THE BEGIN STRING
                int begin_string_offset{-1};
                FixUtilities::find_begin_string_position(buffer+buffer_read_index, buffer_size-buffer_read_index, begin_string_offset);

                if(llfix_likely(begin_string_offset>=0))
                {
                    buffer_read_index += begin_string_offset;
                }
                else
                {
                    LLFIX_LOG_ERROR(m_name + " received a message with no begin string : " + FixUtilities::fix_to_human_readible(buffer+buffer_read_index, buffer_size-buffer_read_index));
                    return;
                }
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                IncomingFixMessage* incoming_fix_message{ nullptr };
                ObjectCache<FixStringView>* fix_string_view_cache{ nullptr };

                if (llfix_likely(session != nullptr))
                {
                    incoming_fix_message = session->get_incoming_fix_message();
                    fix_string_view_cache = session->get_fix_string_view_cache();
                }
                else
                {
                    UnknownSessionContext* unknown_session_context = get_thread_local_unknown_session_context();

                    if (unknown_session_context)
                    {
                        incoming_fix_message = &(unknown_session_context->m_incoming_fix_message);
                        fix_string_view_cache = &(unknown_session_context->m_fix_string_view_cache);
                    }
                    else
                    {
                        this->set_incomplete_buffer(peer_index, buffer, buffer_size);
                        return;
                    }
                }

                incoming_fix_message->reset();
                fix_string_view_cache->reset_pointer();

                current_index = static_cast<int>(buffer_read_index);
                bool looking_for_equals = true;

                int current_tag_start = static_cast<int>(buffer_read_index);
                int current_tag_len{ 0 };

                int current_value_start = static_cast<int>(buffer_read_index);
                int current_value_len{ 0 };

                int current_tag_10_delimiter_index = -1;
                int current_tag_35_tag_start_index = -1;

                uint32_t parser_reject_code = static_cast<uint32_t>(-1);
                uint32_t current_tag_index = 0;

                uint32_t encoded_current_message_type = static_cast<uint32_t>(-1);

                bool in_a_repeating_group{ false };
                uint32_t current_rg_count_tag{ 0 };

                #ifdef LLFIX_ENABLE_BINARY_FIELDS
                int binary_field_length = 0;
                int binary_field_counter = 0;
                #endif

                while (true)
                {
                    char current_char = buffer[current_index];

                    if (looking_for_equals)
                    {
                        if (current_char == FixConstants::FIX_EQUALS)
                        {
                            current_tag_len = current_index - current_tag_start;
                            current_value_start = current_index + 1;
                            looking_for_equals = false;
                        }
                        else if (llfix_unlikely(current_char == FixConstants::FIX_DELIMITER))
                        {
                            current_tag_start = current_index + 1;
                            parser_reject_code = FixParserErrorCodes::NO_EQUALS_SIGN;
                        }
                    }
                    #ifdef LLFIX_ENABLE_BINARY_FIELDS
                    else
                    {
                        if(llfix_unlikely(binary_field_length>0))
                            if(binary_field_counter < binary_field_length)
                                binary_field_counter++;
                    }

                    if (looking_for_equals == false && current_char == FixConstants::FIX_DELIMITER)
                    {
                        bool reached_value_end = true;

                        if(llfix_unlikely(binary_field_length>0))
                        {
                            if(binary_field_length>binary_field_counter)
                            {
                                reached_value_end = false;
                            }
                            else if(binary_field_length == binary_field_counter)
                            {
                                binary_field_length=0;
                                binary_field_counter=0;
                            }
                        }

                        if(llfix_likely(reached_value_end))
                        {

                    #else
                    else if (current_char == FixConstants::FIX_DELIMITER)
                    {
                    #endif
                            current_value_len = current_index - current_value_start;

                            if (llfix_unlikely(current_value_len == 0))
                            {
                                parser_reject_code = FixConstants::FIX_ERROR_CODE_TAG_WITHOUT_VALUE;
                            }

                            bool is_current_tag_numeric{ true };
                            FixSession::validate_tag_format(buffer + current_tag_start, static_cast<std::size_t>(current_tag_len), is_current_tag_numeric, parser_reject_code);

                            if (llfix_likely(is_current_tag_numeric))
                            {
                                ////////////////////////////////////////
                                // RECORD THE CURRENT TAG VALUE PAIR
                                uint32_t tag = Converters::chars_to_unsigned_int<uint32_t>(buffer + current_tag_start, static_cast<std::size_t>(current_tag_len));

                                if(llfix_unlikely(tag == 0))
                                {
                                    parser_reject_code = FixConstants::FIX_ERROR_CODE_INVALID_TAG_NUMBER;
                                }

                                FixStringView* value = fix_string_view_cache->allocate();
                                value->set_buffer(const_cast<char*>(buffer + current_value_start), static_cast<std::size_t>(current_value_len));

                                /////////////////////////////
                                current_tag_index++;
                                FixSession::validate_header_tags_order(tag, current_tag_index, parser_reject_code);
                                /////////////////////////////

                                // Some tags may be used as a group member but also as an individual tag so we can't fully rely on is_a_repeating_group_tag
                                // and need to detect start and end of a group
                                if(encoded_current_message_type != static_cast<uint32_t>(-1))
                                {
                                    if(llfix_likely(!in_a_repeating_group))
                                    {
                                        if (llfix_unlikely(FixSession::get_repeating_group_specs().is_a_repeating_group_count_tag(encoded_current_message_type, tag)))
                                        {
                                            current_rg_count_tag = tag;
                                            in_a_repeating_group = true;
                                        }
                                    }
                                    else
                                    {
                                        if (llfix_likely(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_current_message_type, current_rg_count_tag, tag) == false))
                                        {
                                            in_a_repeating_group = false;
                                        }
                                    }

                                    #ifdef LLFIX_ENABLE_BINARY_FIELDS
                                    if (llfix_unlikely(FixSession::get_binary_field_specs().is_binary_data_length_tag(encoded_current_message_type, tag)))
                                    {
                                        binary_field_length = Converters::chars_to_int<int>(value->data(), value->length());
                                    }
                                    #endif
                                }

                                if (llfix_likely(in_a_repeating_group == false))
                                {
                                    if (llfix_likely(incoming_fix_message->has_tag(tag) == false))
                                    {
                                        incoming_fix_message->set_tag(tag, value);
                                    }
                                    else
                                    {
                                        parser_reject_code = FixConstants::FIX_ERROR_CODE_TAG_APPEARS_MORE_THAN_ONCE;
                                    }
                                }
                                else
                                {
                                    incoming_fix_message->set_repeating_group_tag(tag, value);
                                }

                                if (tag == FixConstants::TAG_CHECKSUM)
                                {
                                    current_tag_10_delimiter_index = current_index;
                                    /////////////////////////////
                                    FixSession::validate_tag9_and_tag35((incoming_fix_message), current_tag_start, current_tag_35_tag_start_index, parser_reject_code);
                                    /////////////////////////////
                                    break;
                                }
                                else if (tag == FixConstants::TAG_MSG_TYPE)
                                {
                                    current_tag_35_tag_start_index = current_tag_start;
                                    encoded_current_message_type = FixUtilities::pack_message_type(value->to_string_view());
                                }
                            } // if (llfix_likely(is_current_tag_numeric))

                            current_tag_start = current_index + 1;
                            looking_for_equals = true;
                        #ifdef LLFIX_ENABLE_BINARY_FIELDS
                        } // if(llfix_likely(reached_value_end))
                        #endif
                    } // else if (current_char == FixConstants::FIX_DELIMITER) or if (looking_for_equals == false && current_char == FixConstants::FIX_DELIMITER)

                    // Apart from the check below (else if (static_cast<int>(buffer_read_index) > final_tag10_delimiter_index)),
                    // we also need to check here if found last tag 10 delimiter is before the found tag8
                    if (current_index >= static_cast<int>(buffer_size) - 1)
                    {
                        this->set_incomplete_buffer(peer_index, buffer + buffer_read_index, buffer_size - buffer_read_index);
                        return;
                    }

                    current_index++;
                } // while

                auto current_message_length = current_tag_10_delimiter_index + 1 - buffer_read_index;
                //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                if (llfix_unlikely(session == nullptr))
                {
                    session = accept_session(peer_index, incoming_fix_message, buffer + buffer_read_index, current_message_length, parser_reject_code);

                    if (session == nullptr) // Means that it was an invalid logon attempt and we already closed the connection
                    {
                        return;
                    }
                }
                //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                session->lock();
                process_incoming_fix_message(session, peer_index, buffer + buffer_read_index, current_message_length, parser_reject_code);
                session->unlock();

                #ifndef LLFIX_UNIT_TEST
                if(llfix_unlikely(session->get_state() == SessionState::DISCONNECTED)) // process_incoming_fix_message may terminate connection
                {
                    return;
                }
                #endif

                buffer_read_index = current_tag_10_delimiter_index + 1;

                if (current_tag_10_delimiter_index == static_cast<int>(buffer_size) - 1)
                {
                    this->reset_incomplete_buffer(peer_index);
                    return;
                }
                else if (static_cast<int>(buffer_read_index) > final_tag10_delimiter_index)
                {
                    this->set_incomplete_buffer(peer_index, buffer + buffer_read_index, buffer_size - buffer_read_index);
                    return;
                }
            }
        }

        void process_invalid_logon(std::size_t peer_index, const std::string& log_message, bool send_logout = false, const std::string& logout_reason_text = "")
        {
            if(!log_message.empty())
            {
                int peer_port{0};
                std::string peer_ip;
                this->get_peer_details(peer_index, peer_ip, peer_port);

                LLFIX_LOG_ERROR("Invalid logon attempt from " + peer_ip + " : " + log_message);
            }

            process_connection_closure(peer_index, send_logout, logout_reason_text);
        }

        void process_connection_closure(std::size_t peer_index, bool send_logout = false, const std::string& logout_reason_text = "")
        {
            if (send_logout)
            {
                auto session = m_fix_connectors.get_session(peer_index);

                if (session != nullptr)
                {
                    send_logout_message(session, logout_reason_text);
                }
            }

            this->close_connection(peer_index);
        }

        void process_incoming_fix_message(FixSession* session, std::size_t peer_index, const char* buffer_message, std::size_t buffer_message_length, uint32_t parser_reject_code)
        {
            session->set_last_received_message_timestamp_nanoseconds(VDSO::nanoseconds_monotonic());

            if(session->serialisation_enabled())
                session->get_incoming_message_serialiser()->write(buffer_message, buffer_message_length, true);

            if (llfix_unlikely(session->expecting_response_for_outgoing_test_request()))
            {
                // We are permissive , any message not just 35=0 with expected t112, satisfies our outgoing test request
                session->set_expecting_response_for_outgoing_test_request(false);
                LLFIX_LOG_DEBUG("FixServer " + m_name + " session " + session->get_name() + " : other end satisfied the test request");
            }
            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (session->validations_enabled())
            {
                uint32_t reject_message_code = static_cast<uint32_t>(-1);

                if (session->validate_fix_message(*session->get_incoming_fix_message(), buffer_message, buffer_message_length, parser_reject_code, reject_message_code) == false)
                {
                    if (reject_message_code != static_cast<uint32_t>(-1))
                    {
                        send_reject_message(session, session->get_incoming_fix_message(), reject_message_code, buffer_message, buffer_message_length, session->get_last_error_tag());
                    }

                    return;
                }
            }
            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // SEQUENCE NO & MESSAGE PERSIST PLUGIN
            auto sequence_store = session->get_sequence_store();
            sequence_store->increment_incoming_seq_no();
            auto sequence_store_incoming_seq_no = sequence_store->get_incoming_seq_no();

            if (m_message_persist_plugin)
                m_message_persist_plugin->persist_incoming_message(session->get_name(), sequence_store_incoming_seq_no, buffer_message, buffer_message_length);

            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // SEQUENCE NO CHECKS
            auto incoming_seq_no = session->get_incoming_fix_message()->get_tag_value_as<uint32_t>(FixConstants::TAG_MSG_SEQ_NUM);

            if (llfix_unlikely(incoming_seq_no > sequence_store_incoming_seq_no))
            {
                if(FixSession::is_a_hard_sequence_reset_message(*session->get_incoming_fix_message()) == false)
                {
                    if (session->get_state() != SessionState::PENDING_LOGON) // on_logon_request handles this case on its own
                    {
                        session->queue_outgoing_resend_request(sequence_store_incoming_seq_no, incoming_seq_no);
                        return;
                    }
                }
            }
            else if (llfix_unlikely(incoming_seq_no < sequence_store_incoming_seq_no))
            {
                if(FixSession::is_a_hard_sequence_reset_message(*session->get_incoming_fix_message()) == false)
                {
                    std::string logout_reason_text;
                    bool send_logout = false;

                    if (session->get_state() == SessionState::PENDING_LOGON)
                    {
                        send_logout = true;
                        logout_reason_text = "MsgSeqNum too low, expecting " + std::to_string(sequence_store_incoming_seq_no) + " but received " + std::to_string(incoming_seq_no);
                    }

                    sequence_store->set_incoming_seq_no(sequence_store_incoming_seq_no - 1);
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " : terminating session " + session->get_name() + " as the incoming sequence no (" + std::to_string(incoming_seq_no) + ") is lower than expected (" + std::to_string(sequence_store_incoming_seq_no) + ")");
                    this->process_connection_closure(peer_index, send_logout, logout_reason_text);
                    return;
                }
            }

            if (llfix_unlikely(session->get_state() == SessionState::IN_RETRANSMISSION_INITIATED_BY_SELF))
            {
                if (incoming_seq_no == session->get_outgoing_resend_request_end_no())
                {
                    LLFIX_LOG_DEBUG("FixServer " + m_name + " : other end satisfied the resend request");
                    session->set_state(llfix::SessionState::LOGGED_ON);
                }
            }
            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if(process_incoming_throttling(session, session->get_incoming_fix_message()) == false)
            {
                return;
            }

            auto message_type = session->get_incoming_fix_message()->get_tag_value(FixConstants::TAG_MSG_TYPE);

            if (llfix_likely(message_type->length() == 1))
            {
                switch (message_type->data()[0])
                {
                    case FixConstants::MSG_TYPE_NEW_ORDER: on_new_order(session, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_ORDER_CANCEL: on_cancel_order(session, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_ORDER_CANCEL_REPLACE: on_replace_order(session, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_HEARTBEAT: on_client_heartbeat(session); break;
                    case FixConstants::MSG_TYPE_TEST_REQUEST: session->process_test_request_message(*session->get_incoming_fix_message()); on_client_test_request(session, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_RESEND_REQUEST: session->process_resend_request(*session->get_incoming_fix_message()); on_client_resend_request(session, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_REJECT: on_session_level_reject(session, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_BUSINESS_REJECT: on_application_level_reject(session, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_LOGON: process_logon_request(session, peer_index, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_LOGOUT: process_logout_request(session, peer_index, session->get_incoming_fix_message()); break;
                    case FixConstants::MSG_TYPE_SEQUENCE_RESET: if (session->process_incoming_sequence_reset_message(*session->get_incoming_fix_message()) == false) { send_reject_message(session, session->get_incoming_fix_message(), FixConstants::FIX_ERROR_CODE_VALUE_INCORRECT_FOR_TAG, buffer_message, buffer_message_length, FixConstants::TAG_NEW_SEQ_NO); }; break;
                        // Anything else
                    default: on_custom_message(session, session->get_incoming_fix_message()); break;
                }
            }
            else
            {
                on_custom_message(session, session->get_incoming_fix_message());
            }
        }

        bool validate_logon_request(FixSession* session, std::size_t peer_index, const IncomingFixMessage* message)
        {
            LLFIX_UNUSED(session);
            int peer_port{0};
            std::string peer_ip;
            this->get_peer_details(peer_index, peer_ip, peer_port);

            if (message->has_tag(FixConstants::TAG_ENCRYPT_METHOD) == false)
            {
                LLFIX_LOG_ERROR("Incoming logon message for server " + m_name + " from " + peer_ip + " does not have required t98(encryption method), compid : " + message->get_tag_value_as<std::string>(FixConstants::TAG_SENDER_COMP_ID));
                return false;
            }

            if (message->has_tag(FixConstants::TAG_HEART_BT_INT) == false)
            {
                LLFIX_LOG_ERROR("Incoming logon message for server " + m_name + " from " + peer_ip + " does not have required t108(heartbeat interval), compid : " + message->get_tag_value_as<std::string>(FixConstants::TAG_SENDER_COMP_ID));
                return false;
            }

            auto t108_string = message->get_tag_value_as<std::string>(FixConstants::TAG_HEART_BT_INT);

            if (t108_string.find('-') != std::string::npos)
            {
                LLFIX_LOG_ERROR("Incoming logon message for server " + m_name + " from " + peer_ip + " has invalid(negative) t108(heartbeat interval) value, compid : " + message->get_tag_value_as<std::string>(FixConstants::TAG_SENDER_COMP_ID));
                return false;
            }

            if (t108_string.find('.') != std::string::npos)
            {
                LLFIX_LOG_ERROR("Incoming logon message for server " + m_name + " from " + peer_ip + " has invalid(decimal points) t108(heartbeat interval) value, compid : " + message->get_tag_value_as<std::string>(FixConstants::TAG_SENDER_COMP_ID));
                return false;
            }

            if (message->get_tag_value_as<uint32_t>(FixConstants::TAG_HEART_BT_INT) == 0)
            {
                LLFIX_LOG_ERROR("Incoming logon message for server " + m_name + " from " + peer_ip + " has invalid(zero) t108(heartbeat interval) value, compid : " + message->get_tag_value_as<std::string>(FixConstants::TAG_SENDER_COMP_ID));
                return false;
            }

            return true;
        }

        void do_post_logon_sequence_number_check(FixSession* session)
        {
            if(session->get_state() == SessionState::LOGGED_ON)
            {
                // IF LOGON WAS WITH A SEQ NO HIGHER THAN EXPECTED , WE NEED TO SEND A RESEND REQUEST
                auto sequence_store_incoming_seq_no = session->get_sequence_store()->get_incoming_seq_no();
                auto incoming_seq_no = session->get_incoming_fix_message()->get_tag_value_as<uint32_t>(FixConstants::TAG_MSG_SEQ_NUM);

                if (llfix_unlikely(incoming_seq_no > sequence_store_incoming_seq_no))
                {
                    if(FixSession::is_a_hard_sequence_reset_message(*session->get_incoming_fix_message()) == false)
                    {
                        session->queue_outgoing_resend_request(sequence_store_incoming_seq_no, incoming_seq_no);
                    }
                }

                // LOW SEQ NOS ARE HANDLED IN process_incoming_fix_message
            }
        }

        void process_logout_request(FixSession* session, std::size_t peer_index, const IncomingFixMessage* message)
        {
            send_logout_message(session);
            session->set_state(SessionState::LOGGED_OUT);
            LLFIX_LOG_DEBUG(m_name + ", session logged out : " + session->get_name() + " , peer index : " + std::to_string(peer_index));
            on_logout_request(session, message);
        }

        FixServer(const FixServer& other) = delete;
        FixServer& operator= (const FixServer& other) = delete;
        FixServer(FixServer&& other) = delete;
        FixServer& operator=(FixServer&& other) = delete;
};

} // namespace