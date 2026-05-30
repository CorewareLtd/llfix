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

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <new>

#include "core/compiler/hints_hot_code.h"
#include "core/compiler/hints_branch_predictor.h"
#include "core/compiler/unused.h"

#include "core/cpu/alignment_constants.h"

#include "core/os/vdso.h"
#include "core/os/thread_utilities.h"
#include "core/os/process_utilities.h"

#include "core/utilities/converters.h"
#include "core/utilities/logger.h"
#include "core/utilities/std_string_utilities.h"

#include "electronic_trading/common/message_persist_plugin.h"

#include "electronic_trading/managed_instance/modifying_admin_command.h"
#include "electronic_trading/managed_instance/managed_instance.h"

#include "fix_constants.h"
#include "incoming_fix_message.h"
#include "outgoing_fix_message.h"
#include "fix_session.h"
#include "fix_session_settings.h"
#include "fix_client_settings.h"
#include "fix_string.h"
#include "fix_string_view.h"
#include "fix_utilities.h"
#include "fix_parser_error_codes.h"

#include "core/utilities/tcp_connector_options.h"

#ifdef LLFIX_UNIT_TEST // VOLTRON_EXCLUDE
#include <cstdlib>
#endif // VOLTRON_EXCLUDE

namespace llfix
{

/**
 * @brief FIX client implementation.
 *
 * @tparam Transport Transport layer implementation: one of TCPConnector TCPConnectorTCPDirect TCPConnectorSSL.
 *
 * FixClient manages a single FIX session lifecycle including connection
 * management, logon/logout, message processing, sequencing, retransmissions,
 * and heartbeats.
 *
 * The class can run in a dedicated internal thread or be driven externally
 * via repeated calls to process().
 *
 * Users are expected to derive from FixClient and override virtual callbacks
 * to handle session and application-level FIX messages.
 */
template<typename Transport>
class FixClient : public Transport, public ManagedInstance
{
    public:

        FixClient() = default;

        virtual ~FixClient()
        {
            if (is_threaded())
            {
                m_is_exiting.store(true);
                m_thread->join();
            }
        }

        /**
         * @brief Create and initialise a FIX client instance.
         *
         * @param client_name Logical name of the FIX client.
         * @param settings Client-level configuration.
         * @param session_name Name of the FIX session.
         * @param session_settings Session-level configuration.
         *
         * @return true on successful creation, false otherwise.
         */
        [[nodiscard]] bool create(const std::string& client_name, const FixClientSettings& settings, const std::string& session_name, const FixSessionSettings& session_settings)
        {
            m_settings = settings;
            return create(client_name, session_name, session_settings);
        }

        /**
         * @brief Create and initialise a FIX client using configuration files.
         *
         * @param client_config_file_path Path to client configuration file.
         * @param client_name Client name inside the config.
         * @param session_config_file_path Path to session configuration file.
         * @param session_name Session name inside the config.
         *
         * @return true on success, false if configuration loading or validation fails.
         */
        [[nodiscard]] bool create(const std::string& client_config_file_path, const std::string& client_name, const std::string& session_config_file_path, const std::string& session_name)
        {
            if (m_settings.load_from_config_file(client_config_file_path, client_name) == false)
            {
                LLFIX_LOG_ERROR("Loading settings for client " + client_name + " failed : " + m_settings.config_load_error);
                return false;
            }

            FixSessionSettings session_settings;
            if (session_settings.load_from_config_file(session_config_file_path, session_name) == false)
            {
                LLFIX_LOG_ERROR("Loading settings for session " + session_name + " failed : " + session_settings.config_load_error);
                return false;
            }

            return create(client_name, session_name, session_settings);
        }

        std::string get_name() const override
        {
            return m_name;
        }

        bool is_instance_ha_primary() const override
        {
            return m_is_ha_primary;
        }

        /**
         * @brief Retrieve the FIX session managed by this client.
         *
         * @return Pointer to the FixSession instance.
         */
        FixSession* get_session(const std::string& session_name="") override
        {
            LLFIX_UNUSED(session_name);
            return &m_session;
        }

        void get_session_names(std::vector<std::string>& target) override
        {
            target.push_back(m_session.get_name());
        }

        void initialise_thread()
        {
            if (m_settings.cpu_core_id >= 0)
            {
                if (ThreadUtilities::pin_calling_thread_to_cpu_core(m_settings.cpu_core_id) == 0)
                {
                    LLFIX_LOG_INFO(m_name + " thread pinned to CPU core " + std::to_string(m_settings.cpu_core_id));
                }
                else
                {
                    LLFIX_LOG_ERROR(m_name + " thread pinning to CPU core " + std::to_string(m_settings.cpu_core_id) + " failed");
                }
            }
        }

        /**
         * @brief Specify repeating group definitions for incoming FIX messages.
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
         * @brief Start the client execution thread.
         *
         * @return true if the thread was successfully created, false otherwise.
         *
         * When started, the client will execute run() in a loop until shutdown
         * is requested.
         */
        [[nodiscard]] bool start()
        {
            try
            {
                m_thread.reset(new std::thread(&FixClient::thread_function, this));
                return true;
            }
            catch(...)
            {}

            LLFIX_LOG_ERROR(m_name + " creation failed during thread creation");
            return false;
        }

        /**
         * @brief Establish a FIX network connection and initiate logon.
         *
         * @return true if connection and logon initiation succeeded, false otherwise.
         *
         * Attempts connection to the primary endpoint first and falls back
         * to the secondary endpoint if configured.
         */
        [[nodiscard]] bool connect()
        {
            if(m_session.get_state() == SessionState::DISABLED)
            {
                LLFIX_LOG_DEBUG("Cannot allow connection attempt as session " + m_session.get_name() + " is disabled.");
                return false;
            }

            if (m_session.is_now_valid_session_datetime() == false)
            {
                LLFIX_LOG_DEBUG("Cannot allow connection attempt for session " + m_session.get_name() + " due to schedule settings.");
                return false;
            }

            m_session.set_state(SessionState::PENDING_CONNECTION);

            m_session.reset_flags();

            TCPConnectorOptions options;

            options.m_rx_buffer_capacity = m_settings.rx_buffer_capacity;
            options.m_receive_size = m_settings.receive_size;
            options.m_send_try_count = m_settings.send_try_count;

            options.m_async_io_timeout_nanoseconds = m_settings.async_io_timeout_nanoseconds;
            options.m_busy_poll_microseconds = m_settings.busy_poll_microseconds;
            options.m_spin_count = m_settings.spin_count;

            options.m_nic_interface_name = m_settings.nic_name.c_str();
            options.m_nic_interface_ip = m_settings.nic_address.c_str();

            options.m_socket_tx_size = m_settings.socket_tx_size;
            options.m_socket_rx_size = m_settings.socket_rx_size;
            options.m_nic_ringbuffer_rx_size = m_settings.nic_ringbuffer_rx_size;
            options.m_nic_ringbuffer_tx_size = m_settings.nic_ringbuffer_tx_size;

            options.m_disable_nagle = m_settings.disable_nagle;
            options.m_enable_quick_ack = m_settings.quick_ack;

            options.m_stack = m_settings.stack;
            options.m_connect_timeout_seconds = m_settings.connect_timeout_seconds;

            #ifdef LLFIX_ENABLE_OPENSSL
            options.m_use_ssl = m_settings.use_ssl;
            options.m_ssl_verify_peer = m_settings.ssl_verify_peer;
            options.m_ssl_ca_pem_file = m_settings.ssl_ca_pem_file;
            options.m_ssl_cert_pem_file = m_settings.ssl_certificate_pem_file;
            options.m_ssl_private_key_pem_file = m_settings.ssl_private_key_pem_file;
            options.m_ssl_private_key_password = m_settings.ssl_private_key_password;
            options.m_ssl_version = m_settings.ssl_version;
            options.m_ssl_cipher_suite = m_settings.ssl_cipher_suite;
            #endif

            Transport::set_params(options);

            bool success = Transport::connect(m_settings.primary_address.c_str(), m_settings.primary_port);
            m_connected_to_primary = success;

            if (success == false)
            {
                // TRY SECONDARY CONNNECTION IF SET
                if (m_settings.secondary_address.length() > 0 && m_settings.secondary_port > 0)
                {
                    LLFIX_LOG_DEBUG(m_name + " : Primary connection failed. Trying secondary connection...");
                    success = Transport::connect(m_settings.secondary_address.c_str(), m_settings.secondary_port);
                    m_connected_to_secondary = success;
                }
            }

            if (success == true)
            {
                m_session.set_last_received_message_timestamp_nanoseconds(VDSO::nanoseconds_monotonic()); // This is to avoid sending unnecessary test request

                LLFIX_LOG_INFO(m_name + " : TCP connection established on " + (m_connected_to_primary?"primary":"secondary") );
                m_session.set_state(SessionState::PENDING_LOGON);
                m_last_logon_attempt_timestamp = VDSO::nanoseconds_monotonic();

                on_connection();

                if (send_logon_request() == false)
                {
                    success = false;
                }
            }
            else
            {
                m_session.set_state(SessionState::DISCONNECTED);
            }

            return success;
        }

        /**
         * @brief Process incoming and outgoing FIX protocol activity.
         *
         * This method drives FIX message handling including:
         * - Incoming message parsing
         * - Admin command processing
         * - Heartbeats and test requests
         * - Resend requests and gap fills
         *
         * Call repeatedly when running without an internal thread.
         */
        void process()
        {
            auto session_state = m_session.get_state();

            if(session_state > SessionState::DISCONNECTED) // In case of BSD sockets, calls on select may hang on some Linux distros if not connected
                this->process_incoming_messages();

            process_admin_commands();

            if (is_state_live(session_state))
            {
                process_outgoing_resend_request_if_necessary(VDSO::nanoseconds_monotonic());
                process_outgoing_test_request_if_necessary(VDSO::nanoseconds_monotonic());

                respond_to_resend_request_if_necessary();
                respond_to_test_request_if_necessary();

                send_client_heartbeat_if_necessary(VDSO::nanoseconds_monotonic());

                process_schedule_validator();
            }
            else if(session_state == SessionState::PENDING_LOGON)
            {
                process_outgoing_logon_request(VDSO::nanoseconds_monotonic());
            }
        }

        void push_admin_command(const std::string& session_name, ModifyingAdminCommandType type, uint32_t arg = 0) override
        {
            LLFIX_UNUSED(session_name);
            auto admin_command = new (std::nothrow) ModifyingAdminCommand;

            if (llfix_likely(admin_command))
            {
                admin_command->type = type;
                admin_command->arg = arg;

                m_session.get_admin_commands()->push(admin_command);
            }
            else
            {
                LLFIX_LOG_ERROR("Failed to process setter admin command for FixClient " + m_name);
            }
        }

        /**
         * @brief Shutdown the FIX client.
         *
         * @param graceful_shutdown If true, performs FIX logout handshake.
         *
         * In threaded mode, this signals the thread to exit.
         * In non-threaded mode, shutdown is performed immediately.
         */
        void shutdown(bool graceful_shutdown = true)
        {
            if(is_threaded())
            {
                m_thread_grace_full_exit.store(graceful_shutdown);
                m_is_exiting.store(true);
                return;
            }

            do_shutdown(graceful_shutdown);
        }

        /**
         * @brief Obtain a reusable outgoing FIX message instance.
         *
         * @return Pointer to an OutgoingFixMessage.
         *
         * The returned message is owned by the session
         */
        OutgoingFixMessage* outgoing_message_instance()
        {
            return m_session.get_outgoing_fix_message();
        }

        /**
         * @brief Encode and send an outgoing FIX message.
         *
         * @param message Outgoing FIX message to send.
         * @return true if the message was successfully sent, false otherwise.
         *
         */
        virtual bool send_outgoing_message(OutgoingFixMessage* message)
        {
            std::size_t encoded_length = 0;
            auto int_sequence_no = m_session.get_sequence_store()->get_outgoing_seq_no() + 1;

            message->encode(m_session.get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, int_sequence_no, encoded_length);

            return send_bytes<true>(m_session.get_tx_encode_buffer(), encoded_length);
        }

        std::string get_settings_as_string(const std::string& delimiter) override
        {
            return m_settings.to_string(delimiter);
        }

        FixClientSettings& get_settings() { return m_settings; }

        /**
         * @brief Check if the client is connected to the primary endpoint.
         *
         * @return true if connected to primary, false otherwise.
         */
        bool connected_to_primary() const { return m_connected_to_primary; }

        /**
         * @brief Check if the client is connected to the secondary endpoint.
         *
         * @return true if connected to secondary, false otherwise.
         */
        bool connected_to_secondary() const { return m_connected_to_secondary; }
        ////////////////////////////////////////////////////////////////////////////////////////////////

        // Run method in case this class is maintaining the client thread
        virtual void run() {};

        // TCP connection
        /**
         * @brief Called when a TCP connection is successfully established.
         */
        virtual void on_connection() {};

        /**
         * @brief Called when the TCP connection is lost or closed.
         */
        virtual void on_disconnection() {};

        virtual void on_async_io_error(int error_code, int event_result) override
        {
            LLFIX_UNUSED(error_code);
            LLFIX_UNUSED(event_result);
        }

        virtual void on_socket_error(int error_code) override
        {
            LLFIX_UNUSED(error_code);
        }
        // Logon messages
        /**
         * @brief Called upon receiving a successful FIX Logon response.
         *
         * @param message Incoming FIX logon message.
         */
        virtual void on_logon_response(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        /**
         * @brief Called upon receiving a FIX Logout response.
         *
         * @param message Incoming FIX logout message.
         */
        virtual void on_logout_response(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        /**
         * @brief Called when a FIX Logon is rejected.
         *
         * @param message Incoming FIX reject message.
         */
        virtual void on_logon_reject(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        // Session/admin messages
        /**
         * @brief Called upon receiving a FIX Heartbeat from the server.
         */
        virtual void on_server_heartbeat() {};
        /**
         * @brief Called upon receiving a FIX Test Request.
         *
         * @param message Incoming FIX test request message.
         */
        virtual void on_server_test_request(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        /**
         * @brief Called upon receiving a FIX Resend Request.
         *
         * @param message Incoming FIX resend request message.
         */
        virtual void on_server_resend_request(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        // Application messages
        /**
         * @brief Called when an Execution Report (35=8) is received.
         *
         * @param message Incoming execution report message.
         */
        virtual void on_execution_report(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        /**
         * @brief Called when an Order Cancel/Replace Reject (35=9) is received.
         *
         * @param message Incoming reject message.
         */
        virtual void on_order_cancel_replace_reject(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        /**
         * @brief Called on session-level FIX rejects.
         *
         * @param message Incoming reject message.
         */
        virtual void on_session_level_reject(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        /**
         * @brief Called on application-level FIX rejects.
         *
         * @param message Incoming reject message.
         */
        virtual void on_application_level_reject(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        /**
         * @brief Called for other FIX message types.
         *
         * @param message Incoming FIX message.
         */
        virtual void on_custom_message_type(const IncomingFixMessage* message) {LLFIX_UNUSED(message);};
        ////////////////////////////////////////////////////////////////////////////////////////////////

    public:

        void on_connection_lost() override
        {
            m_session.set_state(SessionState::DISCONNECTED);
            m_connected_to_primary = false;
            m_connected_to_secondary = false;
            on_disconnection();
        }

        void on_data_ready() override
        {
            std::size_t read = this->receive();

            if (read > 0 && read <= m_settings.rx_buffer_capacity)
            {
                process_rx_buffer(this->get_rx_buffer(), this->get_rx_buffer_size());
            }

            this->receive_done();
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

        #ifdef LLFIX_AUTOMATION
        // Used for testing outgoing msg resends  and gap fills
        bool send_fake_outgoing_message(OutgoingFixMessage* message)
        {
            auto int_sequence_no = m_session.get_sequence_store()->get_outgoing_seq_no() + 1;
            std::size_t encoded_length = 0;

            message->encode(m_session.get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, int_sequence_no, encoded_length);

            auto sequence_store = m_session.get_sequence_store();
            sequence_store->increment_outgoing_seq_no();

            if(m_session.serialisation_enabled())
                m_session.get_outgoing_message_serialiser()->write(reinterpret_cast<const void*>(m_session.get_tx_encode_buffer()), encoded_length, true, sequence_store->get_outgoing_seq_no());

            m_session.set_last_sent_message_timestamp_nanoseconds(VDSO::nanoseconds_monotonic());

            return true;
        }
        #endif

    protected:
        /////////////////////////////////////////////////////////////////////////////////////////////////////
        // ADMIN MESSAGES TO SERVER
        /**
         * @brief Send FIX Logon request.
         *
         * @return true if sent successfully.
         */
        virtual bool send_logon_request()
        {
            auto logon_request = outgoing_message_instance();
            logon_request->set_msg_type(FixConstants::MSG_TYPE_LOGON);

            // TAG 98 ENCRYPTION METHOD
            logon_request->set_tag(FixConstants::TAG_ENCRYPT_METHOD, 0);

            // TAG 108 HEARTBEAT INTERVAL
            logon_request->set_tag(FixConstants::TAG_HEART_BT_INT, static_cast<uint32_t>(m_session.get_heartbeart_interval_in_nanoseconds() / 1'000'000'000));

            // TAG 1137 DEFAULT APP VER ID
            auto default_app_ver_id = m_session.get_default_app_ver_id();

            if (default_app_ver_id.length() > 0)
            {
                logon_request->set_tag(FixConstants::TAG_DEFAULT_APPL_VER_ID, default_app_ver_id);
            }

            // TAG 141 RESET SEQ NOS
            if(m_session.logon_reset_sequence_numbers_flag())
            {
                m_session.get_sequence_store()->reset_numbers();
                logon_request->set_tag(FixConstants::TAG_RESET_SEQ_NUM_FLAG, FixConstants::FIX_BOOLEAN_TRUE);
            }

            // TAG 789 NEXT EXPECTED SEQ NO
            if(m_session.logon_include_next_expected_seq_no())
            {
                const auto next_expected_seq_no = m_session.get_sequence_store()->get_incoming_seq_no()+1;
                logon_request->set_tag(FixConstants::TAG_NEXT_EXPECTED_SEQ_NUM, next_expected_seq_no);
            }

            // TAG 553 SESSION USER NAME
            auto session_username = m_session.get_username();

            if (session_username.length() > 0)
            {
                logon_request->set_tag(FixConstants::TAG_USERNAME, session_username);
            }

            // TAG 554 SESSION PASSWORD
            auto session_password = m_session.get_password();

            if (session_password.length() > 0)
            {
                logon_request->set_tag(FixConstants::TAG_PASSWORD, session_password);
            }

            // TAG 925 NEW PASSWORD
            if (m_session.logon_message_new_password().length() > 0)
            {
                logon_request->set_tag(FixConstants::TAG_NEW_PASSWORD, m_session.logon_message_new_password());
            }

            return send_outgoing_message(logon_request);
        }

        /**
         * @brief Send FIX Logout request.
         *
         * @return true if sent successfully.
         */
        virtual bool send_logout_request()
        {
            m_session.build_logout_message(outgoing_message_instance());
            return send_outgoing_message(outgoing_message_instance());
        }

        /**
         * @brief Send FIX Heartbeat message.
         *
         * @param test_request_id Optional TestReqID.
         * @return true if sent successfully.
         */
        virtual bool send_client_heartbeat(FixString* test_request_id)
        {
            m_session.build_heartbeat_message(outgoing_message_instance(), test_request_id);
            return send_outgoing_message(outgoing_message_instance());
        }

        /**
         * @brief Send FIX Test Request message.
         *
         * @return true if sent successfully.
         */
        virtual bool send_test_request()
        {
            m_session.build_test_request_message(outgoing_message_instance());
            return send_outgoing_message(outgoing_message_instance());
        }

        /**
         * @brief Send FIX Resend Request message.
         *
         * @return true if sent successfully.
         */
        virtual bool send_resend_request()
        {
            m_session.build_resend_request_message(outgoing_message_instance(), "0");
            return send_outgoing_message(outgoing_message_instance());
        }

        /**
         * @brief Send FIX Sequence Reset message.
         *
         * @param desired_sequence_no New sequence number.
         * @return true if sent successfully.
         */
        virtual bool send_sequence_reset_message(uint32_t desired_sequence_no)
        {
            /*
                This message is a "hard" gap fill that doesn't respond to an incoming 35=2/resend request but to an internal admin request
                Therefore we can't set 123=Y
            */
            auto message = outgoing_message_instance();
            m_session.build_sequence_reset_message(message, desired_sequence_no);

            // ENCODE , NO NEED TO INCREMENT OUTGOING SEQ NO AS WE ARE RESETTING WITH THIS MESSAGE
            std::size_t encoded_length = 0;
            message->encode(m_session.get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, desired_sequence_no, encoded_length);

            // SEND
            return send_bytes<false>(m_session.get_tx_encode_buffer(), encoded_length); // false is for not incrementing seq no for this message
        }

        /**
         * @brief Send FIX Gap Fill message.
         *
         * @return true if sent successfully.
         */
        virtual bool send_gap_fill_message()
        {
            /*
                This message is a gap fill that responds to an incoming 35=2/resend request.
                For "hard" gap fills that don't set 123=Y, see send_sequence_reset_message
            */
            auto message = outgoing_message_instance();
            m_session.build_gap_fill_message(message);

            // ENCODE , NO NEED TO INCREMENT OUTGOING SEQ NO AS WE ARE RESETTING WITH THIS MESSAGE
            std::size_t encoded_length = 0;
            message->encode(m_session.get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, m_session.get_incoming_resend_request_begin_no(), encoded_length); // We need to encode with seq no that the peer expects hence using m_incoming_resend_request_begin_no

            // SEND
            return send_bytes<false>(m_session.get_tx_encode_buffer(), encoded_length); // false is for not incrementing seq no for this message
        }

        virtual void resend_messages_to_server(uint32_t begin_seq_no, uint32_t end_seq_no)
        {
            for (uint32_t i = begin_seq_no; i <= end_seq_no; i++)
            {
                std::size_t message_length = 0;
                m_session.get_outgoing_message_serialiser()->read_message(i, m_session.get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, message_length);

                auto message = outgoing_message_instance();
                message->load_from_buffer(m_session.get_tx_encode_buffer(), message_length); // t122(orig sending time) will be handled by load_from_buffer

                message->template set_tag<FixMessageComponent::HEADER, bool>(FixConstants::TAG_POSS_DUP_FLAG, FixConstants::FIX_BOOLEAN_TRUE);

                if (m_session.include_t97_during_resends())
                {
                    message->template set_tag<FixMessageComponent::HEADER, bool>(FixConstants::TAG_POSS_RESEND, FixConstants::FIX_BOOLEAN_TRUE);
                }

                std::size_t encoded_length = 0;
                message->encode(m_session.get_tx_encode_buffer(), m_settings.tx_encode_buffer_capacity, i, encoded_length);
                send_bytes<false>(m_session.get_tx_encode_buffer(), encoded_length); // false is for not incrementing seq no for this message
            }
        }

        virtual bool send_reject_message(const IncomingFixMessage& incoming_message, uint32_t reject_reason_code, const char* buffer_message, std::size_t buffer_message_length, uint32_t error_tag=0)
        {
            LLFIX_UNUSED(incoming_message);
            char reject_reason_text[256];

            std::size_t text_length{ 0 };
            FixUtilities::get_reject_reason_text(reject_reason_text, text_length, reject_reason_code);

            if (error_tag == 0)
            {
                LLFIX_LOG_DEBUG("FixClient " + m_name + " received an invalid message : " + std::string(reject_reason_text) + " ,message : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
            }
            else
            {
                LLFIX_LOG_DEBUG("FixClient " + m_name + " received an invalid message : " + std::string(reject_reason_text) + " ,tag : " + std::to_string(error_tag) + " ,message : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
            }

            m_session.build_session_level_reject_message(outgoing_message_instance(), reject_reason_code, reject_reason_text, error_tag);
            return send_outgoing_message(outgoing_message_instance());
        }

        std::atomic<bool> m_is_exiting = false;

    private:
        LLFIX_ALIGN_DATA(AlignmentConstants::CPU_CACHE_LINE_SIZE) FixSession m_session;

        std::string m_name;
        bool m_is_ha_primary = true;

        static inline constexpr std::size_t MINIMUM_REQUIRED_INITIAL_BUFFER_FOR_PARSING = 4; // 10=<DELIMITER>

        uint64_t m_last_logon_attempt_timestamp = 0;

        bool m_connected_to_primary = false;
        bool m_connected_to_secondary = false;

        FixClientSettings m_settings;

        MessagePersistPlugin* m_message_persist_plugin = nullptr;
        std::atomic<bool> m_thread_grace_full_exit = true;
        std::unique_ptr<std::thread> m_thread;

        bool is_threaded() const { return m_thread.get() != nullptr;}

        void thread_function()
        {
            initialise_thread();

            while (true)
            {
                if (m_is_exiting.load() == true)
                {
                    do_shutdown(m_thread_grace_full_exit.load());
                    break;
                }

                run();
            }
        }

        bool create(const std::string& name, const std::string& session_name, const FixSessionSettings& session_settings)
        {
            m_name = name;
            m_is_ha_primary = m_settings.starts_as_primary_instance;

            if (m_settings.validate() == false)
            {
                LLFIX_LOG_ERROR("FixClientSettings for " + m_name + " validation failed : " + m_settings.validation_error);
                return false;
            }

            session_settings.is_server = false;
            session_settings.tx_encode_buffer_capacity = m_settings.tx_encode_buffer_capacity; // Propagate

            if (m_session.initialise(session_name, session_settings) == false)
            {
                LLFIX_LOG_ERROR(m_name + " creation failed during session initialisation");
                return false;
            }

            if (m_settings.starts_as_primary_instance == false)
            {
                disable_session();
            }

            LLFIX_LOG_INFO(m_name + " : Loaded client config =>\n" + m_settings.to_string());

            LLFIX_LOG_INFO("FixClient " + m_name + " creation success");

            return true;
        }

        void process_admin_commands()
        {
            ModifyingAdminCommand* admin_command{ nullptr };

            if (m_session.get_admin_commands()->try_pop(&admin_command) == true)
            {
                switch (admin_command->type)
                {
                    case ModifyingAdminCommandType::SET_INCOMING_SEQUENCE_NUMBER:
                    {
                        m_session.get_sequence_store()->set_incoming_seq_no(admin_command->arg);
                        LLFIX_LOG_DEBUG(m_name + " : processed set incoming sequence number admin command , new seq no : " + std::to_string(admin_command->arg));
                        break;
                    }

                    case ModifyingAdminCommandType::SET_OUTGOING_SEQUENCE_NUMBER:
                    {
                        m_session.get_sequence_store()->set_outgoing_seq_no(admin_command->arg);
                        LLFIX_LOG_DEBUG(m_name + " : processed set outgoing sequence number admin command , new seq no : " + std::to_string(admin_command->arg));
                        break;
                    }

                    case ModifyingAdminCommandType::SEND_SEQUENCE_RESET:
                    {
                        if( m_session.get_state() == SessionState::LOGGED_ON )
                        {
                            send_sequence_reset_message(admin_command->arg);
                            LLFIX_LOG_DEBUG(m_name + " : processed send sequence reset admin command , new seq no : " + std::to_string(admin_command->arg));
                        }
                        else
                        {
                            LLFIX_LOG_ERROR(m_name + " : dropping send sequence reset admin command as not logged on");
                        }

                        break;
                    }

                    case ModifyingAdminCommandType::DISABLE_SESSION:
                    {
                        disable_session();
                        LLFIX_LOG_DEBUG(m_name + " : processed disable session admin command , new session state : Disabled");
                        break;
                    }

                    case ModifyingAdminCommandType::ENABLE_SESSION:
                    {
                        if(m_is_ha_primary)
                        {
                            enable_session();
                            LLFIX_LOG_DEBUG(m_name + " : processed enable session admin command");
                        }
                        else
                        {
                            LLFIX_LOG_ERROR(m_name + " ignoring enable session admin command since not a primary instance");
                        }
                        break;
                    }

                    case ModifyingAdminCommandType::SET_IS_HA_PRIMARY_INSTANCE:
                    {
                        m_is_ha_primary = admin_command->arg == 1 ? true : false;

                        if(admin_command->arg == 1)
                        {
                            enable_session();
                            LLFIX_LOG_DEBUG(m_name + " : processed set is ha primary instance 1 admin command");
                        }
                        else
                        {
                            disable_session();
                            LLFIX_LOG_DEBUG(m_name + " : processed set is ha primary instance 0 admin command");
                        }

                        break;
                    }

                    default: break;
                }

                delete admin_command;
            }
        }

        void disable_session()
        {
            if(is_state_live(m_session.get_state()))
            {
                do_shutdown(true);
            }
            m_session.set_state(SessionState::DISABLED);
        }

        void enable_session()
        {
            if(m_session.get_state() == SessionState::DISABLED)
            {
                m_session.set_state(SessionState::DISCONNECTED);
                if(m_settings.refresh_resend_cache_during_promotion)
                    m_session.reinitialise_outgoing_serialiser();
            }
        }

        void process_schedule_validator()
        {
            if (m_session.is_now_valid_session_datetime() == false)
            {
                LLFIX_LOG_DEBUG("FixClient " + m_name + " : terminating session due to schedule settings");
                do_shutdown(true);
            }
        }

        void do_shutdown(bool graceful_shutdown = true)
        {
            auto call_time_state = m_session.get_state();
            LLFIX_LOG_DEBUG(m_name + " : shutdown called. Graceful shutdown param : " + (graceful_shutdown?"true":"false") );

            on_connection_lost();

            if (graceful_shutdown)
            {
                if (call_time_state == SessionState::LOGGED_ON)
                {
                    m_session.set_received_logout_response(false);

                    send_logout_request();
                    m_session.set_state(SessionState::PENDING_LOGOUT);

                    auto start = std::chrono::steady_clock::now();
                    const auto timeout = std::chrono::milliseconds(m_session.settings()->logout_timeout_seconds*1000);

                    while(true)
                    {
                        this->process_incoming_messages();

                        if(m_session.received_logout_response() == true)
                        {
                            break;
                        }

                        if (m_session.get_state() == SessionState::DISCONNECTED)
                        {
                            break;
                        }

                        auto now = std::chrono::steady_clock::now();
                        if (now - start >= timeout)
                        {
                            LLFIX_LOG_DEBUG("FixClient " + m_name + " shutdown timed out before receiving logout response");
                            break;
                        }
                    }
                }
            }

            this->close();
            m_session.set_state(SessionState::DISCONNECTED);
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////
        // TO SERVER
        template <bool increment_outgoing_seq_no>
        LLFIX_FORCE_INLINE bool send_bytes(const char* buffer, std::size_t buffer_size)
        {
            auto sequence_store = m_session.get_sequence_store();

            if(llfix_likely(m_session.settings()->throttle_limit != 0))
            {
                m_session.throttler()->update();
                m_session.throttler()->wait();
            }

            auto ret = this->send(buffer, buffer_size);

            if constexpr (increment_outgoing_seq_no == true)
            {
                if(ret == true)
                    sequence_store->increment_outgoing_seq_no();
            }

            if(m_session.serialisation_enabled())
                m_session.get_outgoing_message_serialiser()->write(reinterpret_cast<const void*>(buffer), buffer_size, ret, sequence_store->get_outgoing_seq_no());

            if (m_message_persist_plugin)
                m_message_persist_plugin->persist_outgoing_message(m_session.get_name(), sequence_store->get_outgoing_seq_no(), buffer, buffer_size, ret);

            m_session.set_last_sent_message_timestamp_nanoseconds(VDSO::nanoseconds_monotonic());

            return ret;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////
        // ADMIN UTILITY TO SERVER
        void process_outgoing_logon_request(uint64_t current_timestamp_nanoseconds)
        {
            auto delta_nanoseconds = current_timestamp_nanoseconds - m_last_logon_attempt_timestamp;

            if (delta_nanoseconds >= (static_cast<uint64_t>(m_session.settings()->logon_timeout_seconds) * 1'000'000'000))
            {
                m_session.set_state(SessionState::DISCONNECTED);
            }
        }

        void send_client_heartbeat_if_necessary(uint64_t current_timestamp_nanoseconds)
        {
            auto delta_nanoseconds = current_timestamp_nanoseconds - m_session.last_sent_message_timestamp_nanoseconds();

            if (delta_nanoseconds >= (m_session.get_heartbeart_interval_in_nanoseconds() * static_cast<uint64_t>(9) / static_cast<uint64_t>(10))) // 0.9 for safety
            {
                send_client_heartbeat(nullptr);
            }
        }

        void process_outgoing_test_request_if_necessary(uint64_t current_timestamp_nanoseconds)
        {
            if (m_session.expecting_response_for_outgoing_test_request() == false)
            {
                // SEND IF NECESSARY
                auto delta_nanoseconds = current_timestamp_nanoseconds - m_session.last_received_message_timestamp_nanoseconds();

                if (delta_nanoseconds >= (m_session.get_outgoing_test_request_interval_in_nanoseconds()))
                {
                    LLFIX_LOG_DEBUG("FixClient " + m_name + " : sending test request");
                    send_test_request();
                    m_session.set_outgoing_test_request_timestamp_nanoseconds(current_timestamp_nanoseconds);
                    m_session.set_expecting_response_for_outgoing_test_request(true);
                }
            }
            else
            {
                // TERMINATE SESSION IF NECESSARY
                auto delta_nanoseconds = current_timestamp_nanoseconds - m_session.outgoing_test_request_timestamp_nanoseconds();

                if (delta_nanoseconds >= (m_session.get_heartbeart_interval_in_nanoseconds() * static_cast<uint64_t>(2)))
                {
                    m_session.set_expecting_response_for_outgoing_test_request(false);
                    LLFIX_LOG_DEBUG("FixClient " + m_name + " : terminating session as the other end didn't respond to 35=1/test request");
                    do_shutdown(false);
                }
            }
        }

        void process_outgoing_resend_request_if_necessary(uint64_t current_timestamp_nanoseconds)
        {
            if (m_session.get_state() == SessionState::IN_RETRANSMISSION_INITIATED_BY_SELF)
            {
                // TERMINATE SESSION IF NECESSARY
                uint64_t delta_nanoseconds = current_timestamp_nanoseconds - m_session.outgoing_resend_request_timestamp_nanoseconds();
                uint64_t required_delta_nanoseconds = static_cast<uint64_t>(m_session.outgoing_resend_request_expire_secs()) * 1'000'000'000;

                if (delta_nanoseconds >= required_delta_nanoseconds)
                {
                    // We need to terminate the session as the other side didn't respond properly to our outgoing resend request
                    LLFIX_LOG_DEBUG("FixClient " + m_name + " : terminating session as the other end didn't respond to 35=2/resend request in pre-configured timeout (outgoing_resend_request_expire_secs) : " + std::to_string(m_session.outgoing_resend_request_expire_secs()));
                    do_shutdown(false);
                }
            }
            else
            {
                // SEND IF NECESSARY
                if (m_session.needs_to_send_resend_request())
                {
                    LLFIX_LOG_DEBUG("FixClient " + m_name + " : sending resend request , begin no : " + std::to_string(m_session.get_outgoing_resend_request_begin_no()));
                    send_resend_request();
                    m_session.set_outgoing_resend_request_timestamp_nanoseconds(current_timestamp_nanoseconds);
                    m_session.set_state(llfix::SessionState::IN_RETRANSMISSION_INITIATED_BY_SELF);
                    m_session.set_needs_to_send_resend_request(false);
                }
            }
        }

        void respond_to_test_request_if_necessary()
        {
            if (m_session.needs_responding_to_incoming_test_request())
            {
                if (m_session.get_incoming_test_request_id()->length() > 0)
                {
                    send_client_heartbeat(m_session.get_incoming_test_request_id());
                    m_session.get_incoming_test_request_id()->set_length(0);
                }
                else
                {
                    // We should not hit here , but better than risking disconnection
                    send_client_heartbeat(nullptr);
                }

                LLFIX_LOG_DEBUG("FixClient " + m_name + " : responded to incoming test request");
                m_session.set_needs_responding_to_incoming_test_request(false);
            }
        }

        void respond_to_resend_request_if_necessary()
        {
            if (m_session.needs_responding_to_incoming_resend_request())
            {
                const auto last_outgoing_seq_no = m_session.get_sequence_store()->get_outgoing_seq_no();

                // Partial replays not supported therefore incoming end no should be either 0 or same as last outgoing seq no
                bool will_replay = (m_session.serialisation_enabled()) && (m_session.replay_messages_on_incoming_resend_request() && (m_session.get_incoming_resend_request_end_no() == 0
                                   || m_session.get_incoming_resend_request_end_no() == last_outgoing_seq_no) && m_session.get_incoming_resend_request_begin_no()<=last_outgoing_seq_no);

                if(last_outgoing_seq_no > m_session.get_incoming_resend_request_begin_no())
                    if(last_outgoing_seq_no-m_session.get_incoming_resend_request_begin_no()+1 > m_session.max_resend_range())
                        will_replay = false;

                if(will_replay)
                {
                    for (uint32_t i = m_session.get_incoming_resend_request_begin_no(); i <= last_outgoing_seq_no; i++)
                    {
                        if (m_session.get_outgoing_message_serialiser()->has_message_in_memory(i) == false)
                        {
                            will_replay = false; // We don't have all the messages
                            break;
                        }
                    }
                }

                if(will_replay)
                {
                    LLFIX_LOG_DEBUG("FixClient " + m_name + " : replaying messages , begin seq no : " + std::to_string(m_session.get_incoming_resend_request_begin_no()) + " , end seq no : " + std::to_string(last_outgoing_seq_no));
                    resend_messages_to_server(m_session.get_incoming_resend_request_begin_no(), last_outgoing_seq_no);
                }
                else
                {
                    LLFIX_LOG_DEBUG("FixClient " + m_name + " : sending gap fill message");
                    send_gap_fill_message();
                }

                m_session.set_needs_responding_to_incoming_resend_request(false);
                m_session.set_state(llfix::SessionState::LOGGED_ON);
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////
        // FROM SERVER
        #if defined(LLFIX_BENCHMARK) || defined(LLFIX_UNIT_TEST)
        public:
        #else
        private:
        #endif
        LLFIX_HOT void process_rx_buffer(char* buffer, std::size_t buffer_size)
        {
            std::size_t buffer_read_index = 0;

            if (llfix_unlikely(buffer_size < MINIMUM_REQUIRED_INITIAL_BUFFER_FOR_PARSING))
            {
                this->set_incomplete_buffer(buffer, buffer_size);
                return;
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // FIND OUT THE END
            int final_tag10_delimiter_index{ -1 };
            int current_index = static_cast<int>(buffer_size - 1);

            if(llfix_unlikely(FixUtilities::find_delimiter_from_end(buffer, buffer_size, current_index) == false))
            {
                // We don't have the entire message
                this->set_incomplete_buffer(buffer, buffer_size);
                return;
            }

            FixUtilities::find_tag10_start_from_end(buffer, buffer_size, current_index, final_tag10_delimiter_index);

            if (final_tag10_delimiter_index == -1)
            {
                // We don't have the entire message
                this->set_incomplete_buffer(buffer, buffer_size);
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
                m_session.get_incoming_fix_message()->reset();
                m_session.get_fix_string_view_cache()->reset_pointer();

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

                            if(llfix_unlikely(current_value_len==0))
                            {
                                parser_reject_code = FixConstants::FIX_ERROR_CODE_TAG_WITHOUT_VALUE;
                            }

                            bool is_current_tag_numeric{true};
                            FixSession::validate_tag_format(buffer+current_tag_start, static_cast<std::size_t>(current_tag_len), is_current_tag_numeric, parser_reject_code);

                            if(llfix_likely(is_current_tag_numeric))
                            {
                                ////////////////////////////////////////
                                // RECORD THE CURRENT TAG VALUE PAIR
                                uint32_t tag = Converters::chars_to_unsigned_int<uint32_t>(buffer+ current_tag_start, static_cast<std::size_t>(current_tag_len));

                                if(llfix_unlikely(tag == 0))
                                {
                                    parser_reject_code = FixConstants::FIX_ERROR_CODE_INVALID_TAG_NUMBER;
                                }

                                FixStringView* value = m_session.get_fix_string_view_cache()->allocate();
                                value->set_buffer(const_cast<char*>(buffer + current_value_start), static_cast<std::size_t>(current_value_len));

                                /////////////////////////////
                                current_tag_index++;
                                FixSession::validate_header_tags_order(tag, current_tag_index, parser_reject_code);
                                /////////////////////////////

                                // Some tags may be used as a group member but also as an individual tag so we can't fully rely on is_a_repeating_group_tag
                                // and need to detect start and end of a group
                                if (encoded_current_message_type != static_cast<uint32_t>(-1))
                                {
                                    if (llfix_likely(!in_a_repeating_group))
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
                                    if(llfix_likely(m_session.get_incoming_fix_message()->has_tag(tag) == false))
                                    {
                                        m_session.get_incoming_fix_message()->set_tag(tag, value);
                                    }
                                    else
                                    {
                                        parser_reject_code = FixConstants::FIX_ERROR_CODE_TAG_APPEARS_MORE_THAN_ONCE;
                                    }
                                }
                                else
                                {
                                    m_session.get_incoming_fix_message()->set_repeating_group_tag(tag, value);
                                }

                                if (tag == FixConstants::TAG_CHECKSUM)
                                {
                                    current_tag_10_delimiter_index = current_index;
                                    /////////////////////////////
                                    FixSession::validate_tag9_and_tag35(m_session.get_incoming_fix_message(), current_tag_start, current_tag_35_tag_start_index, parser_reject_code);
                                    /////////////////////////////
                                    break;
                                }
                                else if (tag == FixConstants::TAG_MSG_TYPE)
                                {
                                    current_tag_35_tag_start_index = current_tag_start;
                                    encoded_current_message_type = FixUtilities::pack_message_type(value->to_string_view());
                                }
                            } // if(llfix_likely(is_current_tag_numeric))

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
                        this->set_incomplete_buffer(buffer + buffer_read_index, buffer_size - buffer_read_index);
                        return;
                    }

                    current_index++;
                }// while true
                //////////////////////////////////////////////////////////////////
                auto current_message_length = current_tag_10_delimiter_index+1-buffer_read_index;
                process_incoming_fix_message(m_session.get_incoming_fix_message(), buffer+ buffer_read_index, current_message_length, parser_reject_code);

                #ifndef LLFIX_UNIT_TEST
                if(llfix_unlikely(m_session.get_state() == SessionState::DISCONNECTED)) // process_incoming_fix_message may terminate connection
                {
                    return;
                }
                #endif

                buffer_read_index = current_tag_10_delimiter_index + 1;

                if (current_tag_10_delimiter_index == static_cast<int>(buffer_size) - 1)
                {
                    this->reset_incomplete_buffer();
                    return;
                }
                else if (static_cast<int>(buffer_read_index)> final_tag10_delimiter_index)
                {
                    #ifdef LLFIX_UNIT_TEST
                    this->m_incomplete_buffer = static_cast<char*>(malloc(65536));
                    #endif
                    this->set_incomplete_buffer(buffer + buffer_read_index, buffer_size - buffer_read_index);
                    return;
                }
            }
        }

        void process_incoming_fix_message(IncomingFixMessage* incoming_message, const char* buffer_message, std::size_t buffer_message_length, uint32_t parser_reject_code)
        {
            m_session.set_last_received_message_timestamp_nanoseconds(VDSO::nanoseconds_monotonic());

            if(m_session.serialisation_enabled())
                m_session.get_incoming_message_serialiser()->write(buffer_message, buffer_message_length, true);

            if(llfix_unlikely(m_session.expecting_response_for_outgoing_test_request()))
            {
                // We are permissive , any message not just 35=0 with expected t112, satisfies our outgoing test request
                m_session.set_expecting_response_for_outgoing_test_request(false);
                LLFIX_LOG_DEBUG(m_name + " : other end satisfied the test request");
            }
            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if(m_session.validations_enabled())
            {
                uint32_t reject_message_code = static_cast<uint32_t>(-1);

                if (m_session.validate_fix_message(*m_session.get_incoming_fix_message(), buffer_message, buffer_message_length, parser_reject_code, reject_message_code) == false)
                {
                    if (reject_message_code != static_cast<uint32_t>(-1))
                    {
                        send_reject_message(*m_session.get_incoming_fix_message(), reject_message_code, buffer_message, buffer_message_length, m_session.get_last_error_tag());
                    }

                    return;
                }
            }
            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // SEQUENCE NO & MESSAGE PERSIST PLUGIN
            auto sequence_store = m_session.get_sequence_store();
            sequence_store->increment_incoming_seq_no();
            auto sequence_store_incoming_seq_no = sequence_store->get_incoming_seq_no();

            if (m_message_persist_plugin)
                m_message_persist_plugin->persist_incoming_message(m_session.get_name(), sequence_store_incoming_seq_no, buffer_message, buffer_message_length);

            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // SEQUENCE NO CHECKS
            auto incoming_seq_no = incoming_message->get_tag_value_as<uint32_t>(FixConstants::TAG_MSG_SEQ_NUM);

            if (llfix_unlikely(incoming_seq_no > sequence_store_incoming_seq_no))
            {
                if(FixSession::is_a_hard_sequence_reset_message(*incoming_message) == false)
                {
                    m_session.queue_outgoing_resend_request(sequence_store_incoming_seq_no, incoming_seq_no);
                    return;
                }
            }
            else if (llfix_unlikely(incoming_seq_no < sequence_store_incoming_seq_no))
            {
                if(FixSession::is_a_hard_sequence_reset_message(*incoming_message) == false)
                {
                    sequence_store->set_incoming_seq_no(sequence_store_incoming_seq_no-1);
                    LLFIX_LOG_DEBUG("FixClient " + m_name + " : terminating session as the incoming sequence no (" + std::to_string(incoming_seq_no) + ") is lower than expected ("+ std::to_string(sequence_store_incoming_seq_no) + ")");
                    do_shutdown(false);
                    return;
                }
            }

            if(llfix_unlikely(m_session.get_state() == SessionState::IN_RETRANSMISSION_INITIATED_BY_SELF))
            {
                if(incoming_seq_no == m_session.get_outgoing_resend_request_end_no())
                {
                    m_session.set_state(llfix::SessionState::LOGGED_ON);
                    LLFIX_LOG_DEBUG(m_name + " : other end satisfied the resend request");
                }
            }
            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            auto message_type = incoming_message->get_tag_value(FixConstants::TAG_MSG_TYPE);

            if (llfix_likely(message_type->length() == 1))
            {
                switch (message_type->data()[0])
                {
                    case FixConstants::MSG_TYPE_EXECUTION_REPORT: on_execution_report(incoming_message); break;
                    case FixConstants::MSG_TYPE_HEARTBEAT: on_server_heartbeat(); break;
                    case FixConstants::MSG_TYPE_TEST_REQUEST: m_session.process_test_request_message(*incoming_message); on_server_test_request(incoming_message); break;
                    case FixConstants::MSG_TYPE_RESEND_REQUEST: m_session.process_resend_request(*incoming_message); on_server_resend_request(incoming_message); break;
                    case FixConstants::MSG_TYPE_ORDER_CANCEL_REJECT: on_order_cancel_replace_reject(incoming_message); break;
                    case FixConstants::MSG_TYPE_REJECT: process_session_level_reject(incoming_message); break;
                    case FixConstants::MSG_TYPE_BUSINESS_REJECT: process_application_level_reject(incoming_message); break;
                    case FixConstants::MSG_TYPE_LOGON: process_logon_response(incoming_message); break;
                    case FixConstants::MSG_TYPE_LOGOUT: process_logout_response(incoming_message); break;
                    case FixConstants::MSG_TYPE_SEQUENCE_RESET: if (m_session.process_incoming_sequence_reset_message(*incoming_message) == false) { send_reject_message(*incoming_message, FixConstants::FIX_ERROR_CODE_VALUE_INCORRECT_FOR_TAG, buffer_message, buffer_message_length, FixConstants::TAG_NEW_SEQ_NO); }; break;
                        // Anything else
                    default: on_custom_message_type(incoming_message); break;
                }
            }
            else
            {
                on_custom_message_type(incoming_message);
            }
        }

        void process_session_level_reject(const IncomingFixMessage* message)
        {
            if (m_session.get_state() == SessionState::PENDING_LOGON)
            {
                process_logon_reject(message);
            }
            else
            {
                on_session_level_reject(message);
            }
        }

        void process_application_level_reject(const IncomingFixMessage* message)
        {
            if (m_session.get_state() == SessionState::PENDING_LOGON)
            {
                process_logon_reject(message);
            }
            else
            {
                on_application_level_reject(message);
            }
        }

        void process_logon_response(const IncomingFixMessage* logon_response)
        {
            m_session.set_state(SessionState::LOGGED_ON);
            this->on_logon_response(logon_response);
        }

        void process_logout_response(const IncomingFixMessage* logout_response)
        {
            auto state = m_session.get_state();

            if (state == SessionState::PENDING_LOGON)
            {
                process_logon_reject(logout_response);
            }
            else
            {
                m_session.set_received_logout_response(true);
                m_session.set_state(SessionState::LOGGED_OUT);
                this->on_logout_response(logout_response);
            }
        }

        void process_logon_reject(const IncomingFixMessage* logon_reject)
        {
            m_session.set_state(SessionState::LOGON_REJECTED);

            if (logon_reject->has_tag(FixConstants::TAG_TEXT))
            {
                [[maybe_unused]] auto reject_message = logon_reject->get_tag_value(FixConstants::TAG_TEXT);
                LLFIX_LOG_DEBUG("FixClient " + m_name + " logon message rejected : " + reject_message->to_string());
            }
            else
            {
                LLFIX_LOG_DEBUG("FixClient " + m_name + " logon message rejected with no reason text/tag58.");
            }

            on_logon_reject(logon_reject);
        }

        FixClient(const FixClient& other) = delete;
        FixClient& operator= (const FixClient& other) = delete;
        FixClient(FixClient&& other) = delete;
        FixClient& operator=(FixClient&& other) = delete;
};

} // namespace