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
#include <atomic>
#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <memory>
#include <new>
#include <mutex> // For std::lock_guard

#include <vector>
#include <unordered_map>

#include "core/compiler/hints_hot_code.h"
#include "core/compiler/hints_branch_predictor.h"

#include "core/os/vdso.h"

#include "core/utilities/logger.h"
#include "core/utilities/object_cache.h"
#include "core/utilities/spsc_bounded_queue.h"
#include "core/utilities/std_string_utilities.h"
#include "core/utilities/configuration.h"
#include "core/utilities/userspace_spinlock.h"

#include "electronic_trading/common/message_serialiser.h"

#include "electronic_trading/session/session_state.h"
#include "electronic_trading/session/sequence_store.h"
#include "electronic_trading/session/session_schedule_validator.h"
#include "electronic_trading/session/throttler.h"

#include "electronic_trading/managed_instance/managed_instance_session.h"
#include "electronic_trading/managed_instance/modifying_admin_command.h"

#include "fix_constants.h"
#include "fix_string.h"
#include "fix_string_view.h"
#include "fix_utilities.h"
#include "fix_session_settings.h"
#include "fix_parser_error_codes.h"
#include "incoming_fix_repeating_group_specs.h"
#include "incoming_fix_message.h"
#include "outgoing_fix_message.h"

#ifdef LLFIX_ENABLE_DICTIONARY // VOLTRON_EXCLUDE
#include "fix_dictionary.h"
#include "fix_dictionary_loader.h"
#include "fix_dictionary_validator.h"
#endif // VOLTRON_EXCLUDE

#ifdef LLFIX_ENABLE_BINARY_FIELDS // VOLTRON_EXCLUDE
#include "incoming_fix_binary_field_specs.h"
#endif // VOLTRON_EXCLUDE

namespace llfix
{

using MessageSerialiserType = MessageSerialiser<FixMessageSequenceNumberExtractor>;

/**
 * @class FixSession
 * @brief Represents a single FIX protocol session.
 *
 * FixSession encapsulates the full lifecycle, state management, sequencing,
 * throttling, validation, and message handling logic for a FIX connection.
 *
 */
class FixSession : public ManagedInstanceSession
{
    public :

        FixSession() = default;

        virtual ~FixSession()
        {
            m_sequence_store.save_to_disc();

            if (m_tx_encode_buffer)
            {
                Allocator::deallocate(m_tx_encode_buffer, m_settings.tx_encode_buffer_capacity);
                m_tx_encode_buffer = nullptr;
            }
        }

        bool initialise(const std::string& name, const FixSessionSettings& settings)
        {
            m_name = name;
            m_lock.initialise();

            if(m_name.length()==0)
            {
                LLFIX_LOG_ERROR("Session names can't be empty");
                return false;
            }

            if(m_name.length()>32)
            {
                LLFIX_LOG_ERROR("Session names can have max 32 characters. Failed name : " + m_name);
                return false;
            }

            if (settings.validate() == false)
            {
                LLFIX_LOG_ERROR("FixSessionSettings for session " + m_name + " validation failed : " + settings.validation_error);
                return false;
            }

            try // memory resources
            {
                m_settings = settings;

                m_incoming_throttler_exceed_count = 0;

                set_state(SessionState::DISCONNECTED);

                if (m_settings.schedule_week_days.length() > 0) m_schedule_validator.add_allowed_weekdays_from_dash_separated_string(m_settings.schedule_week_days);
                m_schedule_validator.set_start_and_end_times(m_settings.start_hour_utc, m_settings.start_minute_utc, m_settings.end_hour_utc, m_settings.end_minute_utc);

                if (m_admin_commands.create(128) == false)
                {
                    LLFIX_LOG_ERROR("Failed to create admin commands queue for session " + m_name);
                    return false;
                }

                if(m_settings.throttle_limit>0)
                {
                    if (m_throttler.initialise(m_settings.throttle_window_in_milliseconds * 1'000'000, m_settings.throttle_limit) == false)
                    {
                        LLFIX_LOG_ERROR("Session " + m_name + " : failed to initialise throttler. Check the throttle_limit config.");
                        return false;
                    }
                }

                if (open_or_create_sequence_store(m_settings.sequence_store_file_path) == false)
                {
                    LLFIX_LOG_ERROR("Session " + m_name + " : failed to initialise sequence store. Check the sequence_store_file_path config.");
                    return false;
                }

                if(m_settings.max_serialised_file_size > 0)
                {
                    if (m_incoming_messages_serialiser.initialise(m_settings.incoming_message_serialisation_path, m_settings.max_serialised_file_size, false) == false)
                    {
                        LLFIX_LOG_ERROR("Session " + m_name + " : failed to initialise incoming message serialiser. Check serialisation path and serialised file max size configs.");
                        return false;
                    }

                    if (m_outgoing_messages_serialiser.initialise(m_settings.outgoing_message_serialisation_path, m_settings.max_serialised_file_size, m_settings.replay_messages_on_incoming_resend_request, m_settings.replay_message_cache_initial_size) == false)
                    {
                        LLFIX_LOG_ERROR("Session " + m_name + " : failed to initialise incoming message serialiser. Check serialisation path and serialised file max size configs.");
                        return false;
                    }
                }

                if (m_outgoing_fix_message.initialise(&m_settings, get_sequence_store()) == false)
                {
                    LLFIX_LOG_ERROR("Session " + m_name + " : OutgoingFixMessage creation failed.");
                    return false;
                }

                if (m_incoming_fix_message.initialise() == false)
                {
                    LLFIX_LOG_ERROR("Session " + m_name + " : IncomingFixMessage creation failed.");
                    return false;
                }

                if (m_fix_string_view_cache.create(1024) == false)
                {
                    LLFIX_LOG_ERROR("Session " + m_name + " : FixStringView cache creation failed.");
                    return false;
                }

                m_tx_encode_buffer = reinterpret_cast<char*>(Allocator::allocate(m_settings.tx_encode_buffer_capacity));

                if (m_tx_encode_buffer == nullptr)
                {
                    LLFIX_LOG_ERROR("Session " + m_name + " : TX encode buffer allocation failed");
                    return false;
                }
            }
            catch (const std::bad_alloc&)
            {
                LLFIX_LOG_FATAL("Session " + m_name + " : Insufficient memory.");
                return false;
            }

            try // stoi
            {
                if (m_settings.additional_static_header_tags.length() > 0)
                {
                    auto tag_value_pairs = StringUtilities::split(m_settings.additional_static_header_tags, ',');

                    for (auto pair : tag_value_pairs)
                    {
                        auto tokens = StringUtilities::split(pair, '=');

                        if (tokens.size() != 2)
                        {
                            LLFIX_LOG_ERROR("Invalid FixSession config value for 'additional_static_header_tags' : " + m_settings.additional_static_header_tags);
                            return false;
                        }

                        int tag = std::stoi(tokens[0]);

                        if (tag <= 0)
                        {
                            LLFIX_LOG_ERROR("Invalid FixSession config value for 'additional_static_header_tags' : " + m_settings.additional_static_header_tags + " , tag numbers must be positive integers.");
                            return false;
                        }

                        m_outgoing_fix_message.set_additional_static_header_tag(static_cast<uint32_t>(tag), tokens[1]);
                    }
                }
            }
            catch (...)
            {
                LLFIX_LOG_ERROR("Session " + m_name + " : failed to initialise, check config value for 'additional_static_header_tags'.");
                return false;
            }

            #ifdef LLFIX_ENABLE_DICTIONARY
            bool dictionary_load_success = false;

            try // new s and dict loading
            {
                dictionary_load_success = initialise_dictionary_validator();
            }
            catch(...)
            {}

            if(dictionary_load_success==false)
            {
                LLFIX_LOG_ERROR("Session " + m_name + " : dictionary loading failed.");
                return false;
            }
            #endif

            LLFIX_LOG_INFO("Session " + m_name + " : session config loaded =>\n" + m_settings.to_string());

            return true;
        }

        /**
        * @brief Returns the logical name of the FIX session.
        *
        * The session name uniquely identifies this FixSession instance and is
        * typically configured during initialisation.
        *
        * @return Session name as a std::string.
        */
        std::string get_name() const override { return m_name; }

        FixSessionSettings* settings() { return &m_settings; }

        std::string get_display_text() const
        {
            std::stringstream ret;

            ret << "Session state : " << convert_session_state_to_string(static_cast<SessionState>(m_state.load())) << '\n';
            ret << "Incoming seq no : " << m_sequence_store.get_incoming_seq_no() << '\n';
            ret << "Outgoing seq no : " << m_sequence_store.get_outgoing_seq_no() << '\n';

            return ret.str();
        }

        ////////////////////////////////////////////////////////////
        // STATE

        void set_state(SessionState state)
        {
            m_state = static_cast<int>(state);
        }

        /**
         * @brief Retrieves the current state of the FIX session.
         *
         * The returned value represents the lifecycle and connection state of
         * the session. Possible states are:
         *
         * - SessionState::NONE
         *   Session has not been initialised.
         *
         * - SessionState::DISABLED
         *   Session is administratively disabled.
         *
         * - SessionState::DISCONNECTED
         *   Session is not connected to the peer.
         *
         * - SessionState::LOGGED_OUT
         *   Session is disconnected after a successful logout.
         *
         * - SessionState::LOGON_REJECTED
         *   Logon attempt was rejected by the peer.
         *
         * - SessionState::PENDING_CONNECTION
         *   TCP connection established, waiting to initiate logon.
         *
         * - SessionState::PENDING_LOGON
         *   Logon message sent, awaiting logon response.
         *
         * - SessionState::PENDING_LOGOUT
         *   Logout message sent or received, awaiting completion.
         *
         * - SessionState::LOGGED_ON
         *   Session is logged on and fully operational.
         *
         * - SessionState::IN_RETRANSMISSION_INITIATED_BY_SELF
         *   Session is retransmitting messages initiated by this side.
         *
         * - SessionState::IN_RETRANSMISSION_INITIATED_BY_PEER
         *   Session is retransmitting messages initiated by the peer.
         *
         * @return Current SessionState.
         *
         */
        SessionState get_state()
        {
            return static_cast<SessionState>(m_state.load());
        }

        ////////////////////////////////////////////////////////////
        // SEQUENCE STORE
        bool open_or_create_sequence_store(const std::string_view& path)
        {
            return m_sequence_store.open(path);
        }

        /**
         * @brief Provides access to the session's sequence store.
         *
         * The sequence store maintains incoming and outgoing FIX message
         * sequence numbers and persists them according to configuration.
         *
         * @return Pointer to the underlying SequenceStore.
         */
        SequenceStore* get_sequence_store()
        {
            return &m_sequence_store;
        }

        uint32_t get_last_processed_sequence_number() const { return m_sequence_store.get_incoming_seq_no(); }

        ////////////////////////////////////////////////////////////
        // THROTTLER
        Throttler* throttler() { return &m_throttler; }

        uint32_t get_incoming_throttler_exceed_count() const { return m_incoming_throttler_exceed_count; }
        void increment_incoming_throttler_exceed_count() { m_incoming_throttler_exceed_count++; }

        ////////////////////////////////////////////////////////////
        // SERIALISERS
        MessageSerialiserType* get_incoming_message_serialiser() { return &m_incoming_messages_serialiser; }
        MessageSerialiserType* get_outgoing_message_serialiser() { return &m_outgoing_messages_serialiser; }
        bool serialisation_enabled() const { return m_settings.max_serialised_file_size>0; }

        void reinitialise_outgoing_serialiser()
        {
            LLFIX_LOG_INFO("Session " + m_name + " , record count before reinitialisation : " + std::to_string(m_outgoing_messages_serialiser.get_message_record_count()));
            m_outgoing_messages_serialiser.initialise(m_settings.outgoing_message_serialisation_path, m_settings.max_serialised_file_size, m_settings.replay_messages_on_incoming_resend_request, m_settings.replay_message_cache_initial_size);
            LLFIX_LOG_INFO("Session " + m_name + " , record count after reinitialisation : " + std::to_string(m_outgoing_messages_serialiser.get_message_record_count()));
        }

        ////////////////////////////////////////////////////////////
        // OUTGOING TEST REQUEST
        bool expecting_response_for_outgoing_test_request() const { return m_expecting_response_for_outgoing_test_request; }
        void set_expecting_response_for_outgoing_test_request(bool b) { m_expecting_response_for_outgoing_test_request =b; }

        uint64_t outgoing_test_request_timestamp_nanoseconds() const { return m_outgoing_test_request_timestamp_nanoseconds; }
        void set_outgoing_test_request_timestamp_nanoseconds(uint64_t val) { m_outgoing_test_request_timestamp_nanoseconds = val; }

        ////////////////////////////////////////////////////////////
        // OUTGOING RESEND REQUEST
        bool needs_to_send_resend_request() const { return m_needs_to_send_resend_request; }
        void set_needs_to_send_resend_request(bool b) { m_needs_to_send_resend_request = b; }

        uint32_t get_outgoing_resend_request_begin_no() const { return m_outgoing_resend_request_begin_no; }
        uint32_t get_outgoing_resend_request_end_no() const { return m_outgoing_resend_request_end_no; }

        void queue_outgoing_resend_request(uint32_t sequence_store_incoming_seq_no, uint32_t live_incoming_seq_no)
        {
            m_needs_to_send_resend_request = true;
            m_sequence_store.set_incoming_seq_no(sequence_store_incoming_seq_no - 1);
            m_outgoing_resend_request_begin_no = sequence_store_incoming_seq_no;
            m_outgoing_resend_request_end_no = live_incoming_seq_no;
        }

        uint64_t outgoing_resend_request_timestamp_nanoseconds() const { return m_outgoing_resend_request_timestamp_nanoseconds; }
        void set_outgoing_resend_request_timestamp_nanoseconds(uint64_t val) { m_outgoing_resend_request_timestamp_nanoseconds = val; }

        ////////////////////////////////////////////////////////////
        // INCOMING RESEND REQUEST
        bool needs_responding_to_incoming_resend_request() const { return m_needs_responding_to_incoming_resend_request; }
        void set_needs_responding_to_incoming_resend_request(bool b) { m_needs_responding_to_incoming_resend_request = b; }

        uint32_t get_incoming_resend_request_begin_no() const { return m_incoming_resend_request_begin_no; }
        uint32_t get_incoming_resend_request_end_no() const { return m_incoming_resend_request_end_no; }

        ////////////////////////////////////////////////////////////
        // INCOMING TEST REQUEST
        bool needs_responding_to_incoming_test_request() const { return m_needs_responding_to_incoming_test_request; }
        void set_needs_responding_to_incoming_test_request(bool b) { m_needs_responding_to_incoming_test_request = b; }

        FixString* get_incoming_test_request_id() { return &m_incoming_test_request_id; }

        ////////////////////////////////////////////////////////////
        // LOGOUTS
        bool received_logout_response() const { return m_received_logout_response; }
        void set_received_logout_response(bool b) { m_received_logout_response = b; }

        ////////////////////////////////////////////////////////////
        // TIMESTAMPS
        uint64_t last_received_message_timestamp_nanoseconds() const { return m_last_received_message_timestamp_nanoseconds; }
        void set_last_received_message_timestamp_nanoseconds(uint64_t val) { m_last_received_message_timestamp_nanoseconds = val; }

        uint64_t last_sent_message_timestamp_nanoseconds() const { return m_last_sent_message_timestamp_nanoseconds; }
        void set_last_sent_message_timestamp_nanoseconds(uint64_t val) { m_last_sent_message_timestamp_nanoseconds = val; }

        ////////////////////////////////////////////////////////////
        // ATTRIBUTES
        /**
         * @brief Adds or updates a session-scoped attribute.
         *
         * Attributes are stored as key/value string pairs and can be used to
         * associate arbitrary metadata with a session instance.
         *
         * Supported value types:
         * - std::string
         * - Arithmetic types (converted via std::to_string)
         * - Any type supporting stream insertion (operator<<)
         *
         * @tparam T Attribute value type.
         * @param attribute Attribute name (key).
         * @param value Attribute value.
         *
         * @return true if the attribute was added successfully, false otherwise.
         */
        template <typename T>
        bool add_attribute(const std::string& attribute, const T& value)
        {
            bool ret{true};

            try
            {
                if constexpr (std::is_same_v<T, std::string>)
                {
                    m_attributes.add_attribute(attribute, value);
                }
                else if constexpr (std::is_arithmetic_v<T>)
                {
                    m_attributes.add_attribute(attribute, std::to_string(value));
                }
                else
                {
                    std::ostringstream oss;
                    oss << value;
                    m_attributes.add_attribute(attribute, oss.str());
                }
            }
            catch(...)
            {
                ret = false;
            }

            return ret;
        }

        /**
         * @brief Retrieves a session attribute value.
         *
         * Looks up an attribute by name and returns its value if present.
         *
         * @param attribute Attribute name (key).
         * @param value Output parameter receiving the attribute value.
         *
         * @return true if the attribute exists, false otherwise.
         */
        bool get_attribute(const std::string& attribute, std::string& value) const
        {
            if(m_attributes.does_attribute_exist(attribute))
            {
                value = m_attributes.get_string_value(attribute);
                return true;
            }

            return false;
        }

        ////////////////////////////////////////////////////////////
        // SETTINGS GENERAL
        uint64_t get_heartbeart_interval_in_nanoseconds() const { return m_settings.heartbeat_interval_in_nanoseconds; }

        uint64_t get_outgoing_test_request_interval_in_nanoseconds() const
        {
            return m_settings.outgoing_test_request_interval_in_nanoseconds;
        }

        bool enable_simd_avx2() const { return m_settings.enable_simd_avx2; }
        VDSO::SubsecondPrecision get_timestamp_subsecond_precision() const { return m_settings.timestamp_subseconds_precision; }

        void set_heartbeart_interval_in_nanoseconds(uint64_t value)
        {
            m_settings.heartbeat_interval_in_nanoseconds = value;
        }

        void set_outgoing_test_request_interval_in_nanoseconds(uint64_t value)
        {
            m_settings.outgoing_test_request_interval_in_nanoseconds = value;
        }

        void set_enable_simd_avx2(bool b) { m_settings.enable_simd_avx2 = b; }

        int get_protocol_version() const { return m_settings.protocol_version; }

        ////////////////////////////////////////////////////////////
        // SETTINGS HEADERS
        const std::string& get_begin_string() const { return m_settings.begin_string; }
        const std::string& get_compid() const { return m_settings.sender_comp_id; }
        const std::string& get_target_compid() const { return m_settings.target_comp_id; }
        bool include_last_processed_seqnum_in_header() const { return m_settings.include_last_processed_seqnum_in_header; }

        void set_begin_string(const std::string& str) { m_settings.begin_string = str; }
        void set_compid(const std::string_view& identifier) { m_settings.sender_comp_id = identifier; }
        void set_target_compid(const std::string_view& identifier) { m_settings.target_comp_id = identifier; }

        ////////////////////////////////////////////////////////////
        // SETTINGS LOGONS
        const std::string& get_default_app_ver_id() const { return m_settings.default_app_ver_id; }
        const std::string& get_username() const { return m_settings.logon_username; }
        const std::string& get_password() const { return m_settings.logon_password; }
        std::string logon_message_new_password() const { return m_settings.logon_message_new_password; }
        bool logon_reset_sequence_numbers_flag() const { return m_settings.logon_reset_sequence_numbers; }
        bool logon_include_next_expected_seq_no() const { return m_settings.logon_include_next_expected_seq_no; }

        ////////////////////////////////////////////////////////////
        // SETTINGS VALIDATIONS
        bool validations_enabled() const { return m_settings.validations_enabled; }
        void set_validations_enabled(bool b) { m_settings.validations_enabled = b; }

        bool validate_repeating_groups_enabled() const { return m_settings.validate_repeating_groups; }
        void set_validate_repeating_groups_enabled(bool b) { m_settings.validate_repeating_groups = b; }

        ////////////////////////////////////////////////////////////
        // SETTINGS INCOMING RESEND REQUESTS
        bool replay_messages_on_incoming_resend_request() const { return m_settings.replay_messages_on_incoming_resend_request; }
        bool include_t97_during_resends() const { return m_settings.include_t97_during_resends; }
        std::size_t max_resend_range() const { return m_settings.max_resend_range; }
        void set_replay_messages_on_incoming_resend_request (bool b) { m_settings.replay_messages_on_incoming_resend_request=b; }

        ////////////////////////////////////////////////////////////
        // SETTINGS OUTGOING RESEND REQUESTS
        int outgoing_resend_request_expire_secs() const { return m_settings.outgoing_resend_request_expire_in_secs; }

        ////////////////////////////////////////////////////////////////////
        // ADMIN MESSAGE BUILDERS
        void build_heartbeat_message(OutgoingFixMessage* message, FixString* test_request_id)
        {
            message->set_msg_type(FixConstants::MSG_TYPE_HEARTBEAT);

            if (test_request_id != nullptr)
            {
                message->set_tag(FixConstants::TAG_TEST_REQ_ID, test_request_id);
            }
        }

        void build_test_request_message(OutgoingFixMessage* message)
        {
            message->set_msg_type(FixConstants::MSG_TYPE_TEST_REQUEST);
            m_outgoing_test_request_id++;
            message->set_tag(FixConstants::TAG_TEST_REQ_ID, m_outgoing_test_request_id);
        }

        void build_resend_request_message(OutgoingFixMessage* message, const std::string& end_no)
        {
            message->set_msg_type(FixConstants::MSG_TYPE_RESEND_REQUEST);
            message->set_tag(FixConstants::TAG_BEGIN_SEQ_NO, m_outgoing_resend_request_begin_no);
            message->set_tag(FixConstants::TAG_END_SEQ_NO, end_no);
        }

        void build_logout_message(OutgoingFixMessage* message, const std::string& reason_text = "")
        {
            message->set_msg_type(FixConstants::MSG_TYPE_LOGOUT);

            if (reason_text.length() > 0)
            {
                message->set_tag(FixConstants::TAG_TEXT, reason_text);
            }
        }

        void build_session_level_reject_message(OutgoingFixMessage* message, uint32_t reject_reason_code, const char* reject_reason_text, uint32_t error_tag=0)
        {
            message->set_msg_type(FixConstants::MSG_TYPE_REJECT);

            if (m_incoming_fix_message.has_tag(FixConstants::TAG_MSG_SEQ_NUM))
            {
                message->set_tag(FixConstants::TAG_REF_SEQ_NUM, m_incoming_fix_message.get_tag_value_as<uint32_t>(FixConstants::TAG_MSG_SEQ_NUM));
            }

            if(error_tag != 0)
            {
                message->set_tag(FixConstants::TAG_REF_TAG, error_tag);
            }

            if (m_incoming_fix_message.has_tag(FixConstants::TAG_MSG_TYPE))
            {
                message->set_tag(FixConstants::TAG_REF_MSG_TYPE, m_incoming_fix_message.get_tag_value_as<std::string>(FixConstants::TAG_MSG_TYPE));
            }

            message->set_tag(FixConstants::TAG_SESSION_REJECT_REASON, reject_reason_code);
            message->set_tag(FixConstants::TAG_TEXT, reject_reason_text);
        }

        void build_gap_fill_message(OutgoingFixMessage* message)
        {
            message->set_msg_type(FixConstants::MSG_TYPE_SEQUENCE_RESET);

            // TAG36/NEW SEQ NO
            // We don't support partial gap fill.
            // Any ResendRequest is answered by fast-forwarding the counterparty
            // to the next outbound sequence number (last sent + 1).
            const auto next_outgoing_sequence_no = m_sequence_store.get_outgoing_seq_no() + 1;
            message->set_tag(FixConstants::TAG_NEW_SEQ_NO, next_outgoing_sequence_no);

            // OPTIONAL TAG 123/GAP FILL FLAG
            message->set_tag(FixConstants::TAG_GAP_FILL_FLAG, FixConstants::FIX_BOOLEAN_TRUE);
        }

        void build_sequence_reset_message(OutgoingFixMessage* message, uint32_t desired_sequence_no)
        {
            m_sequence_store.set_outgoing_seq_no(desired_sequence_no);

            // MSG TYPE
            message->set_msg_type(FixConstants::MSG_TYPE_SEQUENCE_RESET);

            // TAG36/NEW SEQ NO
            message->set_tag(FixConstants::TAG_NEW_SEQ_NO, desired_sequence_no + 1); // +1 is becasue send functions will increment by 1 so the other side should expect +1
        }

        ////////////////////////////////////////////////////////////////////
        // RX PROCESSING METHODS
        bool process_incoming_sequence_reset_message(const IncomingFixMessage& message)
        {
            // We don't distinguish between hard gap fills ( without 123=Y, initiated from the other side )
            // and soft gap fills (with 123=Y, responses to our outgoing 35=2s)
            if (message.has_tag(FixConstants::TAG_NEW_SEQ_NO))
            {
                if (message.is_tag_value_numeric(FixConstants::TAG_NEW_SEQ_NO))
                {
                    LLFIX_LOG_DEBUG("Session " + m_name + " : incoming sequence reset message tag36(new seq no) : " + message.get_tag_value_as<std::string>(FixConstants::TAG_NEW_SEQ_NO));

                    auto new_incoming_seq_no = message.get_tag_value_as<uint32_t>(FixConstants::TAG_NEW_SEQ_NO);

                    if(new_incoming_seq_no==0)
                    {
                        LLFIX_LOG_DEBUG("Session " + m_name + " : received invalid new tag sequence number(0) for sequence reset.");
                        return false;
                    }

                    if (new_incoming_seq_no <= m_sequence_store.get_incoming_seq_no()) // <= because process_incoming_fix_message will do +1
                    {
                        LLFIX_LOG_DEBUG("Session " + m_name + " : the sequence reset can only increase the sequence number.");
                        return false;
                    }

                    m_sequence_store.set_incoming_seq_no(new_incoming_seq_no - 1); // t36 is what next seq no will be , applying -1 as process_incoming_fix_message will do +1
                    set_state(SessionState::LOGGED_ON);
                }
                else
                {
                    LLFIX_LOG_DEBUG("Session " + m_name + " : incoming sequence reset message has invalid tag36(new seq no).");
                }
            }
            else
            {
                LLFIX_LOG_DEBUG("Session " + m_name + " : incoming sequence reset message does not have tag36(new seq no).");
            }

            return true;
        }

        void process_resend_request(const IncomingFixMessage& message)
        {
            if (message.has_tag(FixConstants::TAG_BEGIN_SEQ_NO) && message.has_tag(FixConstants::TAG_END_SEQ_NO))
            {
                bool is_tag_7_numeric = message.is_tag_value_numeric(FixConstants::TAG_BEGIN_SEQ_NO);
                bool is_tag_16_numeric = message.is_tag_value_numeric(FixConstants::TAG_END_SEQ_NO);

                if (is_tag_7_numeric && is_tag_16_numeric)
                {
                    LLFIX_LOG_DEBUG("Session " + m_name + " : incoming resend request tag7(begin seq no) : " + message.get_tag_value_as<std::string>(FixConstants::TAG_BEGIN_SEQ_NO) + " tag16(end seq no) : " + message.get_tag_value_as<std::string>(FixConstants::TAG_END_SEQ_NO));

                    set_state(llfix::SessionState::IN_RETRANSMISSION_INITIATED_BY_PEER);

                    m_incoming_resend_request_begin_no = message.get_tag_value_as<uint32_t>(FixConstants::TAG_BEGIN_SEQ_NO);

                    m_incoming_resend_request_end_no = message.get_tag_value_as<uint32_t>(FixConstants::TAG_END_SEQ_NO);

                    m_needs_responding_to_incoming_resend_request = true;
                }
                else
                {
                    if (is_tag_7_numeric == false)
                    {
                        LLFIX_LOG_DEBUG("Session " + m_name + " : incoming resend request message has invalid tag7(begin seq no).");
                    }

                    if (is_tag_16_numeric == false)
                    {
                        LLFIX_LOG_DEBUG("Session " + m_name + " : incoming resend request message has invalid tag16(end seq no).");
                    }
                }
            }
            else
            {
                LLFIX_LOG_DEBUG("Session " + m_name + " : incoming resend request message does not have one or both of tag7(begin seq no) and tag16(end seq no).");
            }
        }

        void process_test_request_message(const IncomingFixMessage& message)
        {
            LLFIX_LOG_DEBUG("Session " + m_name + " : incoming test request");

            if (m_expecting_response_for_outgoing_test_request == false)
            {
                m_needs_responding_to_incoming_test_request = true;

                if (message.has_tag(FixConstants::TAG_TEST_REQ_ID))
                {
                    auto test_request_id = message.get_tag_value(FixConstants::TAG_TEST_REQ_ID);
                    m_incoming_test_request_id.copy_from(test_request_id->data(), test_request_id->length());
                }
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////
        // VALIDATION METHODS
        bool is_now_valid_session_datetime() const
        {
            return m_schedule_validator.is_now_valid_datetime();
        }

        static void validate_header_tags_order(uint32_t tag, uint32_t tag_index, uint32_t& parser_reject_code)
        {
            if (tag_index < 4)
            {
                if (tag_index == 1)
                {
                    if (llfix_unlikely(tag != FixConstants::TAG_BEGIN_STRING))
                    {
                        parser_reject_code = FixParserErrorCodes::OUT_OF_ORDER_HEADER_FIELDS;
                    }
                }
                else if (tag_index == 2)
                {
                    if (llfix_unlikely(tag != FixConstants::TAG_BODY_LENGTH))
                    {
                        parser_reject_code = FixParserErrorCodes::OUT_OF_ORDER_HEADER_FIELDS;
                    }
                }
                else if (tag_index == 3)
                {
                    if (llfix_unlikely(tag != FixConstants::TAG_MSG_TYPE))
                    {
                        parser_reject_code = FixParserErrorCodes::OUT_OF_ORDER_HEADER_FIELDS;
                    }
                }
            }
        }

        static void validate_tag_format(const char* buffer, std::size_t buffer_len, bool& is_numeric, uint32_t& parser_reject_code)
        {
            if (llfix_unlikely(buffer_len == 0 || buffer_len>10))
            {
                parser_reject_code = FixConstants::FIX_ERROR_CODE_INVALID_TAG_NUMBER;
                is_numeric = false;
                return;
            }

            for (std::size_t i = 0; i < buffer_len; i++)
            {
                char c = buffer[i];

                if (llfix_unlikely(c < '0' || c > '9'))
                {
                    parser_reject_code = FixConstants::FIX_ERROR_CODE_INVALID_TAG_NUMBER;
                    is_numeric = false;
                    return;
                }
            }
        }

        static void validate_tag9_and_tag35(IncomingFixMessage* incoming_message, int tag_10_start_index, int tag_35_tag_start_index, uint32_t& parser_reject_code)
        {
            if (llfix_likely(tag_35_tag_start_index != -1))
            {
                if (llfix_likely(incoming_message->has_tag(FixConstants::TAG_BODY_LENGTH)))
                {
                    if (llfix_likely(incoming_message->is_tag_value_numeric(FixConstants::TAG_BODY_LENGTH)))
                    {
                        auto msg_body_len_val = incoming_message->get_tag_value_as<uint32_t>(FixConstants::TAG_BODY_LENGTH);

                        if (llfix_unlikely(static_cast<int>(msg_body_len_val) != (tag_10_start_index - tag_35_tag_start_index)))
                        {
                            parser_reject_code = FixParserErrorCodes::WRONG_BODY_LENGTH;
                        }
                    }
                    else
                    {
                        parser_reject_code = FixParserErrorCodes::INVALID_TAG_9;
                    }
                }
                else
                {
                    parser_reject_code = FixParserErrorCodes::NO_TAG_9;
                }
            }
            else
            {
                parser_reject_code = FixParserErrorCodes::NO_TAG_35;
            }
        }

        bool validate_fix_message(const IncomingFixMessage& incoming_message, const char* buffer_message, std::size_t buffer_message_length, uint32_t reject_reason_code, uint32_t& out_reject_message_code)
        {
            // TAG34 EXISTENCE
            if (incoming_message.has_tag(FixConstants::TAG_MSG_SEQ_NUM) == false)
            {
                set_last_error_tag(0);
                LLFIX_LOG_DEBUG("Session " + m_name + " received a message with no tag34(sequence number) : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                return false;
            }

            // TAG34 FORMAT
            if (incoming_message.is_tag_value_numeric(FixConstants::TAG_MSG_SEQ_NUM) == false)
            {
                set_last_error_tag(0);
                LLFIX_LOG_DEBUG("Session " + m_name + " received a message with invalid tag34(sequence number) : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                return false;
            }

            // ISSUES FOUND DURING PARSING
            if (llfix_unlikely(reject_reason_code != static_cast<uint32_t>(-1)))
            {
                set_last_error_tag(0);

                if (reject_reason_code > FixConstants::FIX_MAX_ERROR_CODE) // Codes above 99 are our internal ones
                {
                    std::string log_message = "Session " + m_name + " " + FixParserErrorCodes::get_internal_parser_error_description(reject_reason_code) + " : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length);
                    LLFIX_LOG_DEBUG(log_message);
                }
                else
                {
                    out_reject_message_code = reject_reason_code;
                }

                return false;
            }

            // TAG8 EXISTENCE
            if (incoming_message.has_tag(FixConstants::TAG_BEGIN_STRING) == false)
            {
                set_last_error_tag(0);
                LLFIX_LOG_DEBUG("Session " + m_name + " received a message with no tag8(begin string) : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                return false;
            }

            // FIX PROTOCOL VERSION
            if (!incoming_message.get_tag_value(FixConstants::TAG_BEGIN_STRING)->equals(m_settings.begin_string.c_str()))
            {
                set_last_error_tag(0);
                LLFIX_LOG_DEBUG("Session " + m_name + " received a message with invalid begin string : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                return false;
            }

            // TAG49 EXISTENCE
            if (incoming_message.has_tag(FixConstants::TAG_SENDER_COMP_ID) == false)
            {
                set_last_error_tag(0);
                LLFIX_LOG_DEBUG("Session " + m_name + " received a message with no tag49(compid) : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                return false;
            }

            // COMPID VALIDATION
            if (!incoming_message.get_tag_value(FixConstants::TAG_SENDER_COMP_ID)->equals(m_settings.target_comp_id.c_str()))
            {
                set_last_error_tag(0);
                out_reject_message_code = FixConstants::FIX_ERROR_CODE_COMPID_PROBLEM;
                return false;
            }

            // TAG56 EXISTENCE
            if (incoming_message.has_tag(FixConstants::TAG_TARGET_COMP_ID) == false)
            {
                set_last_error_tag(0);
                LLFIX_LOG_DEBUG("Session " + m_name + " received a message with no tag56(target compid) : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                return false;
            }

            // TARGET COMPID VALIDATION
            if (!incoming_message.get_tag_value(FixConstants::TAG_TARGET_COMP_ID)->equals(m_settings.sender_comp_id.c_str()))
            {
                set_last_error_tag(0);
                out_reject_message_code = FixConstants::FIX_ERROR_CODE_COMPID_PROBLEM;
                return false;
            }

            // TAG10 FORMAT
            if (incoming_message.is_tag_value_numeric(FixConstants::TAG_CHECKSUM) == false)
            {
                set_last_error_tag(0);
                LLFIX_LOG_DEBUG("Session " + m_name + " received a message with invalid tag10(checksum) : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                return false;
            }

            // CHECKSUM VALIDATION
            const uint32_t actual_checksum = incoming_message.get_tag_value_as<uint32_t>(FixConstants::TAG_CHECKSUM);

            if (m_settings.enable_simd_avx2)
            {
                if (FixUtilities::validate_checksum_simd_avx2(buffer_message, buffer_message_length - 7, actual_checksum) == false) // 7 is length of tag10 + its value + = + SOH
                {
                    set_last_error_tag(0);
                    LLFIX_LOG_DEBUG("Session " + m_name + " received a message with invalid checksum : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                    return false;
                }
            }
            else
            {
                if (FixUtilities::validate_checksum_no_simd(buffer_message, buffer_message_length - 7, actual_checksum) == false) // 7 is length of tag10 + its value + = + SOH
                {
                    set_last_error_tag(0);
                    LLFIX_LOG_DEBUG("Session " + m_name + " received a message with invalid checksum : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                    return false;
                }
            }

            // TAG52 EXISTENCE
            if (incoming_message.has_tag(FixConstants::TAG_SENDING_TIME) == false)
            {
                set_last_error_tag(0);
                LLFIX_LOG_DEBUG("Session " + m_name + " received a message with no tag52(sending time) : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                return false;
            }

            // STALENESS VALIDATION
            if(m_settings.max_allowed_message_age_seconds > 0)
            {
                auto sending_time = incoming_message.get_tag_value(FixConstants::TAG_SENDING_TIME);

                if(sending_time->is_timestamp() == false)
                {
                    set_last_error_tag(FixConstants::TAG_SENDING_TIME);
                    out_reject_message_code = FixConstants::FIX_ERROR_CODE_FORMAT_INCORRECT_FOR_TAG;
                    LLFIX_LOG_DEBUG("Session " + m_name + " received an invalid timestamp : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                    return false;
                }

                if(FixUtilities::is_utc_timestamp_stale(sending_time->to_string_view(), static_cast<int>(m_settings.max_allowed_message_age_seconds)))
                {
                    set_last_error_tag(0);
                    out_reject_message_code = FixConstants::FIX_ERROR_CODE_SENDING_TIME_ACCURACY_PROBLEM;
                    LLFIX_LOG_DEBUG("Session " + m_name + " received a stale message : " + FixUtilities::fix_to_human_readible(buffer_message, buffer_message_length));
                    return false;
                }
            }

            #ifdef LLFIX_ENABLE_DICTIONARY
            // DICTIONARY VALIDATIONS
            if (m_settings.dictionary_validations_enabled == true)
            {
                if (m_validator_id >= 0)
                {
                    if (get_validator(m_validator_id).validate(m_incoming_fix_message, out_reject_message_code) == false)
                    {
                        set_last_error_tag(get_validator(m_validator_id).get_last_error_tag());
                        return false;
                    }

                    // DICTIONARY VALIDATIONS - REPEATING GROUPS
                    if (validate_repeating_groups_enabled())
                    {
                        if (get_validator(m_validator_id).validate_repeating_groups(m_incoming_fix_message, out_reject_message_code) == false)
                        {
                            set_last_error_tag(get_validator(m_validator_id).get_last_error_tag());
                            return false;
                        }
                    }
                }
            }
            #endif

            return true;
        }

        uint32_t get_last_error_tag() const { return m_last_error_tag; }

        ////////////////////////////////////////////////////////////////////
        // OTHERS
        IncomingFixMessage* get_incoming_fix_message() { return &m_incoming_fix_message; }
        OutgoingFixMessage* get_outgoing_fix_message() { return &m_outgoing_fix_message; }

        ObjectCache<FixStringView>* get_fix_string_view_cache() { return &m_fix_string_view_cache; }
        char* get_tx_encode_buffer() { return m_tx_encode_buffer; }
        SPSCBoundedQueue<ModifyingAdminCommand*>* get_admin_commands() { return &m_admin_commands; }

        void reset_flags()
        {
            m_needs_responding_to_incoming_test_request = false;
            m_expecting_response_for_outgoing_test_request = false;
            m_needs_responding_to_incoming_resend_request = false;
            m_needs_to_send_resend_request = false;
            m_received_logout_response = false;

            m_incoming_throttler_exceed_count = 0;
        }

        static bool is_a_hard_sequence_reset_message(const IncomingFixMessage& message)
        {
            if (message.has_tag(FixConstants::TAG_MSG_TYPE) == false)
            {
                return false; // Not even a valid message
            }

            if (message.get_tag_value_as<char>(FixConstants::TAG_MSG_TYPE) != FixConstants::MSG_TYPE_SEQUENCE_RESET)
            {
                return false; // Not even a sequence reset message
            }

            if (message.has_tag(FixConstants::TAG_GAP_FILL_FLAG))
            {
                if (message.get_tag_value_as<char>(FixConstants::TAG_GAP_FILL_FLAG) == FixConstants::FIX_BOOLEAN_TRUE)
                {
                    return false; // Then it is a soft gap fill
                }
                else
                {
                    return true;
                }
            }
            else
            {
                return true;
            }
        }

        void lock()
        {
            m_lock.lock();
        }

        void unlock()
        {
            m_lock.unlock();
        }

        static IncomingFixRepeatingGroupSpecs& get_repeating_group_specs()
        {
            return m_repeating_group_specs;
        }

        #ifdef LLFIX_ENABLE_DICTIONARY
        LLFIX_FORCE_INLINE static FixDictionaryValidator::Validator& get_validator(uint32_t validator_id)
        {
            assert(m_validators.find(validator_id) != m_validators.end());
            return m_validators[validator_id];
        }
        #endif

        #ifdef LLFIX_ENABLE_BINARY_FIELDS
        static IncomingFixBinaryFieldSpecs& get_binary_field_specs()
        {
            return m_binary_field_specs;
        }
        #endif

    private:
        std::atomic<int> m_state = static_cast<int>(SessionState::NONE);
        std::string m_name;
        SequenceStore m_sequence_store;
        Throttler m_throttler;
        MessageSerialiserType m_outgoing_messages_serialiser;
        MessageSerialiserType m_incoming_messages_serialiser;
        Configuration m_attributes;

        FixSessionSettings m_settings;

        uint64_t m_last_sent_message_timestamp_nanoseconds = 0;
        uint64_t m_last_received_message_timestamp_nanoseconds = 0;
        uint32_t m_incoming_throttler_exceed_count = 0;

        char* m_tx_encode_buffer = nullptr;

        OutgoingFixMessage m_outgoing_fix_message;

        IncomingFixMessage m_incoming_fix_message;
        ObjectCache<FixStringView> m_fix_string_view_cache;

        UserspaceSpinlock<> m_lock;

        // INCOMING TEST REQUEST RELATED
        FixString m_incoming_test_request_id;
        bool m_needs_responding_to_incoming_test_request = false;

        // OUTGOING TEST REQUEST RELATED
        uint32_t m_outgoing_test_request_id = 0;
        uint64_t m_outgoing_test_request_timestamp_nanoseconds = 0;
        bool m_expecting_response_for_outgoing_test_request = false;

        // RESEND REQUEST/ MESSAGE REPLAYING RELATED ( WHEN INITIATED BY THE SERVER SIDE )
        bool m_needs_responding_to_incoming_resend_request = false;
        uint32_t m_incoming_resend_request_begin_no = 0;
        uint32_t m_incoming_resend_request_end_no = 0;

        // RESEND REQUEST/ MESSAGE REPLAYING RELATED ( WHEN INITIATED BY SELF )
        bool m_needs_to_send_resend_request = false;
        uint32_t m_outgoing_resend_request_begin_no = 0;
        uint32_t m_outgoing_resend_request_end_no = 0;

        uint64_t m_outgoing_resend_request_timestamp_nanoseconds = 0;

        // LOGOUT RELATED
        bool m_received_logout_response = false;

        // ADMIN COMMANDS
        SPSCBoundedQueue<ModifyingAdminCommand*> m_admin_commands;

        // VALIDATIONS
        SessionScheduleValidator m_schedule_validator;
        uint32_t m_last_error_tag=0;

        static inline IncomingFixRepeatingGroupSpecs m_repeating_group_specs;

        #ifdef LLFIX_ENABLE_BINARY_FIELDS
        static inline IncomingFixBinaryFieldSpecs m_binary_field_specs;
        #endif

        #ifdef LLFIX_ENABLE_DICTIONARY
        static inline UserspaceSpinlock<> m_validators_initialisation_lock;
        static inline std::unordered_map<std::string, int> m_path_validator_id_table;
        static inline int m_latest_validator_id = 0;
        static inline std::unordered_map<int, FixDictionaryValidator::Validator> m_validators;

        int m_validator_id = -1;
        std::unique_ptr<std::vector<std::string>> m_dictionary_load_errors;

        void on_dictionary_load_error(const std::string& message)
        {
            m_dictionary_load_errors->push_back(message);
        }

        bool initialise_dictionary_validator()
        {
            const std::lock_guard<UserspaceSpinlock<>> lock(m_validators_initialisation_lock);

            if (m_settings.application_dictionary_path.length() == 0)
                return true;

            //////////////////////////////////////////////////////////////////////////////////////
            // CHECK IF ALREADY LOADED
            bool already_loaded_dictionary = false;

            if (m_path_validator_id_table.find(m_settings.application_dictionary_path) != m_path_validator_id_table.end())
            {
                m_validator_id = m_path_validator_id_table[m_settings.application_dictionary_path];
                already_loaded_dictionary = true;
            }
            else
            {
                m_latest_validator_id++;
                m_validator_id = m_latest_validator_id;
            }
            //////////////////////////////////////////////////////////////////////////////////////
            // LOAD DICTIONARIES
            m_dictionary_load_errors.reset(new std::vector<std::string>);
            std::unique_ptr<FixDictionaryLoader> dictionary_loader(new FixDictionaryLoader);

            auto get_dictionary_load_errors = [&]()
                {
                    std::string ret;

                    auto dictionary_load_errors = *dictionary_loader->errors();

                    for (const auto& error : dictionary_load_errors)
                    {
                        ret += error + "\n";
                    }

                    return ret;
                };

            if (m_settings.transport_dictionary_path.length() > 0)
            {
                if (dictionary_loader->load_from(m_settings.transport_dictionary_path, true) == false)
                {
                    LLFIX_LOG_ERROR("Session " + m_name + " transport dictionary loading errors :" + get_dictionary_load_errors());
                    return false;
                }
            }

            if (m_settings.application_dictionary_path.length() > 0)
            {
                if (dictionary_loader->load_from(m_settings.application_dictionary_path, false) == false)
                {
                    LLFIX_LOG_ERROR("Session " + m_name + " application dictionary loading errors :" + get_dictionary_load_errors());
                    return false;
                }
            }
            //////////////////////////////////////////////////////////////////////////////////////
            // CHECK BEGIN STRING
            std::string dictionary_begin_string;

            if (dictionary_loader->get_dictionary()->get_begin_string(dictionary_begin_string))
            {
                if (m_settings.begin_string != dictionary_begin_string)
                {
                    LLFIX_LOG_ERROR("Session " + m_name + " : begin string specified in config file (" + m_settings.begin_string + ") does not match the dictionary (" + dictionary_begin_string + ")");
                    return false;
                }
            }
            else
            {
                LLFIX_LOG_ERROR("Session " + m_name + " failed to retrieve begin string from dictionary.");
                return false;
            }
            //////////////////////////////////////////////////////////////////////////////////////
            // LOAD VALIDATOR
            FixDictionaryValidator::Validator validator;

            if (already_loaded_dictionary == false)
            {
                auto fields = *dictionary_loader->get_dictionary()->fields();

                // 1. FIELD DEFINITIONS
                for (const auto& field : fields)
                {
                    FixDictionaryValidator::FieldDefinition field_definition;
                    field_definition.m_type = get_validator_field_type(field.second.type);

                    if (field_definition.m_type != FixDictionaryValidator::FieldType::NONE)
                    {
                        for (const auto& allowed_value : field.second.values)
                        {
                            field_definition.add_allowed_value(allowed_value);
                        }

                        validator.specify_field_definition(field.second.tag, field_definition);
                    }
                }

                // 2. MESSAGE DEFINITIONS
                load_validator_message_definitions(validator, dictionary_loader->get_dictionary(), fields);

                // 3. REPEATING GROUP DEFINITIONS
                load_validator_repeating_group_definitions(validator, dictionary_loader->get_dictionary(), fields);

                #ifdef LLFIX_ENABLE_BINARY_FIELDS
                // 4. BINARY FIELDS DEFINITIONS
                load_binary_fields(dictionary_loader->get_dictionary(), fields);
                #endif
            }

            // CHECK DICTIONARY ERRORS
            for (const auto& dict_error : *m_dictionary_load_errors)
            {
                LLFIX_LOG_ERROR("Session " + m_name + " dictionary error : " + dict_error);
            }

            if (m_dictionary_load_errors->size() > 0)
            {
                return false;
            }
            //////////////////////////////////////////////////////////////////////////////////////
            // SAVE VALIDATOR
            if (already_loaded_dictionary == false)
            {
                m_validators[m_validator_id] = validator;
                m_path_validator_id_table[m_settings.application_dictionary_path] = m_validator_id;
            }

            return true;
        }

        void get_all_non_repeating_group_fields_of_component_recursively(std::unordered_map<std::string, Component>& components, const std::string& component_name, bool is_parent_required, std::vector<MessageField>& target, int& depth)
        {
            depth++;

            if (components.find(component_name) != components.end())
            {
                Component& component = components[component_name];

                for (const auto& field : component.fields)
                {
                    MessageField cp;
                    cp.name = field.name;

                    // A field in a component may be required. But if at the same time its parent is non-required,
                    // then we modify requiredness of a field
                    cp.required = field.required && is_parent_required;

                    target.push_back(cp);
                }

                for (const auto& component_field : component.components)
                {
                    get_all_non_repeating_group_fields_of_component_recursively(components, component_field.name, component_field.required && is_parent_required, target, depth);
                    depth--;
                }
            }
            else
            {
                on_dictionary_load_error("Could not find component " + component_name);
            }
        }

        void load_validator_message_definitions(FixDictionaryValidator::Validator& target_validator, FixDictionary* dictionary, std::unordered_map<std::string, Field>& fields)
        {
            auto messages = dictionary->messages();
            auto components = *dictionary->components();
            auto header = dictionary->header();
            auto trailer = dictionary->trailer();

            auto add_field_to_message_definition = [&](const std::string& field_name, bool required, FixDictionaryValidator::MessageDefinition& target)
                {
                    if (fields.find(field_name) != fields.end())
                    {
                        if (required)
                        {
                            auto current_tag = fields[field_name].tag;
                            if(current_tag != FixConstants::TAG_BEGIN_STRING && current_tag != FixConstants::TAG_BODY_LENGTH && current_tag != FixConstants::TAG_MSG_SEQ_NUM && current_tag != FixConstants::TAG_MSG_TYPE && current_tag != FixConstants::TAG_SENDER_COMP_ID && current_tag != FixConstants::TAG_SENDING_TIME && current_tag != FixConstants::TAG_TARGET_COMP_ID) // Already being checked by parser & validate_fix_message
                                target.add_required_field({ current_tag });
                            else
                                target.add_non_required_field({ current_tag }); // Still need to add them not to fail "is this tag for this message" validation
                        }
                        else
                        {
                            target.add_non_required_field({ fields[field_name].tag });
                        }
                    }
                    else
                    {
                        on_dictionary_load_error("Could not find field " + field_name);
                    }
                };

            std::vector<MessageField> header_component_fields;
            for (const auto& component : header->components)
            {
                int depth = 0;
                get_all_non_repeating_group_fields_of_component_recursively(components, component.name, component.required, header_component_fields, depth);
            }

            std::vector<MessageField> trailer_component_fields;
            for (const auto& component : trailer->components)
            {
                int depth = 0;
                get_all_non_repeating_group_fields_of_component_recursively(components, component.name, component.required, trailer_component_fields, depth);
            }

            for (const auto& message : *messages)
            {
                auto message_type = message.second.msg_type;
                FixDictionaryValidator::MessageDefinition message_definition;

                // MESSAGE FIELDS
                for (auto& field : message.second.fields)
                {
                    add_field_to_message_definition(field.name, field.required, message_definition);
                }

                // (NON REPEATING GROUP) COMPONENT FIELDS
                for (const auto& component : message.second.components)
                {
                    std::vector<MessageField> component_fields;
                    int depth = 0;
                    get_all_non_repeating_group_fields_of_component_recursively(components, component.name, component.required, component_fields, depth);

                    for (const auto& field : component_fields)
                    {
                        add_field_to_message_definition(field.name, (component.required && field.required), message_definition);
                    }
                }

                // HEADER FIELDS FOR ALL MSG TYPES
                for (const auto& field : header->fields)
                {
                    if (fields.find(field.name) != fields.end())
                    {
                        add_field_to_message_definition(field.name, field.required, message_definition);
                    }
                    else
                    {
                        on_dictionary_load_error("Could not find field " + field.name);
                    }
                }

                // HEADER (NON REPEATING GROUP) COMPONENT FIELDS FOR ALL MSG TYPES
                for (const auto& field : header_component_fields)
                {
                    if (fields.find(field.name) != fields.end())
                    {
                        add_field_to_message_definition(field.name, field.required, message_definition);
                    }
                    else
                    {
                        on_dictionary_load_error("Could not find field " + field.name);
                    }
                }

                // TRAILER FIELDS FOR ALL MSG TYPES
                for (const auto& field : trailer->fields)
                {
                    if (fields.find(field.name) != fields.end())
                    {
                        add_field_to_message_definition(field.name, field.required, message_definition);
                    }
                    else
                    {
                        on_dictionary_load_error("Could not find field " + field.name);
                    }
                }

                // TRAILER (NON REPEATING GROUP) COMPONENT FIELDS FOR ALL MSG TYPES
                for (const auto& field : trailer_component_fields)
                {
                    if (fields.find(field.name) != fields.end())
                    {
                        add_field_to_message_definition(field.name, field.required, message_definition);
                    }
                    else
                    {
                        on_dictionary_load_error("Could not find field " + field.name);
                    }
                }

                target_validator.specify_message_definition(message_type, message_definition);
            }
        }

        #ifdef LLFIX_ENABLE_BINARY_FIELDS
        void load_binary_fields(FixDictionary* dictionary, std::unordered_map<std::string, Field>& fields)
        {
            auto messages = *dictionary->messages();
            auto header = *dictionary->header();
            auto trailer = *dictionary->trailer();
            auto components = *dictionary->components();

            for (auto& message : messages)
            {
                // MESSAGE'S OWN FIELDS
                process_field_vector(message.second.fields, message.first, fields);

                // MESSAGE COMPONENTS
                for (auto component : message.second.components)
                {
                    process_component_binary_fields_recursively(component, components, fields, message.first);
                }

                // MESSAGE GROUP FIELDS
                for (auto group_field : message.second.groups)
                {
                    process_group_binary_fields_recursively(group_field, components, fields, message.first);
                }

                // HEADER FIELDS
                process_field_vector(header.fields, message.first, fields);

                // HEADER COMPONENTS
                for (auto component : header.components)
                {
                    process_component_binary_fields_recursively(component, components, fields, message.first);
                }

                // HEADER GROUP FIELDS
                for (auto group : header.groups)
                {
                    process_group_binary_fields_recursively(group, components, fields, message.first);
                }

                // TRAILER FIELDS
                process_field_vector(trailer.fields, message.first, fields);

                // HEADER COMPONENTS
                for (auto component : trailer.components)
                {
                    process_component_binary_fields_recursively(component, components, fields, message.first);
                }

                // TRAILER GROUP FIELDS
                for (auto group : trailer.groups)
                {
                    process_group_binary_fields_recursively(group, components, fields, message.first);
                }
            }
        }

        void process_component_binary_fields_recursively(ComponentField& component, std::unordered_map<std::string, Component>& components, std::unordered_map<std::string, Field>& fields, const std::string& message_type)
        {
            if (components.find(component.name) != components.end())
            {
                process_field_vector(components[component.name].fields, message_type, fields);

                for (auto& nested_component : components[component.name].components)
                {
                    process_component_binary_fields_recursively(nested_component, components, fields, message_type);
                }

                for (auto& nested_group_field : components[component.name].groups)
                {
                    process_group_binary_fields_recursively(nested_group_field, components, fields, message_type);
                }

            }
            else
            {
                on_dictionary_load_error("Could not find component " + component.name);
            }
        }

        void process_group_binary_fields_recursively(GroupField& group_field, std::unordered_map<std::string, Component>& components, std::unordered_map<std::string, Field>& fields, const std::string& message_type)
        {
            process_field_vector(group_field.fields, message_type, fields);

            for (auto& nested_component : group_field.component_fields)
            {
                process_component_binary_fields_recursively(nested_component, components, fields, message_type);
            }

            for (auto& nested_group_field : group_field.group_fields)
            {
                process_group_binary_fields_recursively(nested_group_field, components, fields, message_type);
            }
        }

        void process_field_vector(std::vector<MessageField>& field_vector, const std::string& message_type, std::unordered_map<std::string, Field>& fields)
        {
            int index{ 0 };
            uint32_t current_length_tag = 0;
            int current_length_index = 0;

            for (auto& field : field_vector)
            {
                index++;

                if (fields.find(field.name) != fields.end())
                {
                    auto current_field_type = fields[field.name].type;

                    if (current_field_type == FieldType::LENGTH)
                    {
                        current_length_tag = fields[field.name].tag;
                        current_length_index = index;
                    }
                    else if (current_field_type == FieldType::DATA)
                    {
                        if (index == current_length_index + 1)
                        {
                            m_binary_field_specs.specify_binary_field(message_type, current_length_tag, fields[field.name].tag);
                        }
                    }
                }
                else
                {
                    on_dictionary_load_error("Could not find field " + field.name);
                }
            }
        }
        #endif

        void load_validator_repeating_group_definitions(FixDictionaryValidator::Validator& target_validator, FixDictionary* dictionary, std::unordered_map<std::string, Field>& fields)
        {
            auto messages = *dictionary->messages();
            auto header = *dictionary->header();
            auto trailer = *dictionary->trailer();
            auto components = *dictionary->components();

            std::vector<uint32_t> count_tags_stack;
            count_tags_stack.reserve(4);

            for (const auto& message : messages)
            {
                // MESSAGE'S OWN COMPONENTS
                for (const auto& component : message.second.components)
                {
                    if (components.find(component.name) != components.end())
                    {
                        int depth{ 0 };
                        process_rg_component_tags_recursively(target_validator, components, fields, count_tags_stack, message.first, components[component.name], component.required, depth);
                    }
                    else
                    {
                        on_dictionary_load_error("Could not find component " + component.name);
                    }
                }

                // HEADER COMPONENTS
                for (const auto& component : header.components)
                {
                    if (components.find(component.name) != components.end())
                    {
                        int depth{ 0 };
                        process_rg_component_tags_recursively(target_validator, components, fields, count_tags_stack, message.first, components[component.name], component.required, depth);
                    }
                    else
                    {
                        on_dictionary_load_error("Could not find component " + component.name);
                    }
                }

                // TRAILER COMPONENTS
                for (const auto& component : trailer.components)
                {
                    if (components.find(component.name) != components.end())
                    {
                        int depth{ 0 };
                        process_rg_component_tags_recursively(target_validator, components, fields, count_tags_stack, message.first, components[component.name], component.required, depth);
                    }
                    else
                    {
                        on_dictionary_load_error("Could not find component " + component.name);
                    }
                }

                // MESSAGE'S OWN GROUP FIELDS
                for (const auto& group_field : message.second.groups)
                {
                    int depth{ 0 };
                    process_group_field_rg_tags_recursively(target_validator, components, fields, count_tags_stack, message.first, group_field, group_field.required, depth);
                }

                // HEADER GROUP FIELDS
                for (const auto& group_field : header.groups)
                {
                    int depth{ 0 };
                    process_group_field_rg_tags_recursively(target_validator, components, fields, count_tags_stack, message.first, group_field, group_field.required, depth);
                }

                // TRAILER GROUP FIELDS
                for (const auto& group_field : trailer.groups)
                {
                    int depth{ 0 };
                    process_group_field_rg_tags_recursively(target_validator, components, fields, count_tags_stack, message.first, group_field, group_field.required, depth);
                }
            }
        }

        // RG component : a component with at least one rg definition either directly or in its nested components
        void process_rg_component_tags_recursively(FixDictionaryValidator::Validator& target_validator, std::unordered_map<std::string, Component>& components, std::unordered_map<std::string, Field>& fields, std::vector<uint32_t>& count_tags_stack, const std::string& message_type, const Component& component, bool is_parent_required, int& depth)
        {
            depth++;

            for (const auto& group : component.groups)
            {
                int group_traversal_depth{ 0 };
                process_group_field_rg_tags_recursively(target_validator, components, fields, count_tags_stack, message_type, group, group.required && is_parent_required, group_traversal_depth);
            }

            for (const auto& nested_component_field : component.components)
            {
                if (components.find(nested_component_field.name) != components.end())
                {
                    process_rg_component_tags_recursively(target_validator, components, fields, count_tags_stack, message_type, components[nested_component_field.name], nested_component_field.required && is_parent_required, depth);
                    depth--;
                }
                else
                {
                    on_dictionary_load_error("Could not find component " + nested_component_field.name);
                }
            }
        }

        // Non RG component : a component with no rg definitions but only fields
        void process_non_rg_component_tags_recursively(FixDictionaryValidator::Validator& target_validator, std::unordered_map<std::string, Component>& components, std::unordered_map<std::string, Field>& fields, std::vector<uint32_t>& count_tags_stack, const std::string& message_type, const Component& component, bool is_parent_required, int& depth)
        {
            depth++;

            for (const auto& field : component.fields)
            {
                process_repeating_group_tag(target_validator, fields, count_tags_stack, field.name, message_type, field.required && is_parent_required, false);
            }

            for (const auto& nested_component : component.components)
            {
                if (components.find(nested_component.name) != components.end())
                {
                    process_non_rg_component_tags_recursively(target_validator, components, fields, count_tags_stack, message_type, components[nested_component.name], nested_component.required && is_parent_required, depth);
                    depth--;
                }
                else
                {
                    on_dictionary_load_error("Could not find component " + nested_component.name);
                }
            }
        }

        void process_group_field_rg_tags_recursively(FixDictionaryValidator::Validator& target_validator, std::unordered_map<std::string, Component>& components, std::unordered_map<std::string, Field>& fields, std::vector<uint32_t>& count_tags_stack, const std::string& message_type, const GroupField& group_field , bool is_parent_required, int& depth)
        {
            depth++;

            auto count_tag = fields[group_field.name].tag;

            count_tags_stack.push_back(count_tag);

            process_repeating_group_tag(target_validator, fields, count_tags_stack, group_field.name, message_type, group_field.required && is_parent_required, true);

            for (const auto& field : group_field.fields)
            {
                process_repeating_group_tag(target_validator, fields, count_tags_stack, field.name, message_type, field.required && is_parent_required, false);
            }

            for (const auto& nested_component : group_field.component_fields)
            {
                if (components.find(nested_component.name) != components.end())
                {
                    int nested_component_traversal_depth{ 0 };
                    process_rg_component_tags_recursively(target_validator, components, fields, count_tags_stack, message_type, components[nested_component.name], nested_component.required && is_parent_required, nested_component_traversal_depth);

                    nested_component_traversal_depth = 0;
                    process_non_rg_component_tags_recursively(target_validator, components, fields, count_tags_stack, message_type, components[nested_component.name], nested_component.required && is_parent_required, nested_component_traversal_depth);
                }
                else
                {
                    on_dictionary_load_error("Could not find component " + nested_component.name);
                }
            }

            for (const auto& nested_group : group_field.group_fields)
            {
                process_group_field_rg_tags_recursively(target_validator, components, fields, count_tags_stack, message_type, nested_group, nested_group.required && is_parent_required, depth);
                depth--;
            }

            count_tags_stack.pop_back();
        }

        void process_repeating_group_tag(FixDictionaryValidator::Validator& target_validator, std::unordered_map<std::string, Field>& fields, std::vector<uint32_t>& count_tags_stack, const std::string& field_name, const std::string& message_type, bool required, bool is_count_tag)
        {
            if (fields.find(field_name) != fields.end())
            {
                uint32_t current_tag = fields[field_name].tag;

                if (is_count_tag)
                {
                    m_repeating_group_specs.add_count_tag(message_type, current_tag);

                    // If any other count tag is already in the stack, it means that this is a nested repeating group. In that case we need to add this tag as a repeating group tag for all parent count tags in the stack (except itself)
                    for (auto it = count_tags_stack.begin(); it != count_tags_stack.end() - 1; ++it)
                    {
                        m_repeating_group_specs.add_repeating_group_tag(message_type, *it, current_tag);
                    }

                    if (required)
                    {
                        target_validator.specify_repeating_group_count_tag(message_type, current_tag);
                    }
                }
                else
                {
                    for (const auto& parent_count_tag : count_tags_stack)
                    {
                        m_repeating_group_specs.add_repeating_group_tag(message_type, parent_count_tag, current_tag);
                    }
                }
            }
            else
            {
                on_dictionary_load_error("Could not find field " + field_name);
            }
        }

        static FixDictionaryValidator::FieldType get_validator_field_type(FieldType type)
        {
            switch (type)
            {
                // CHAR
                case FieldType::CHAR: return FixDictionaryValidator::FieldType::CHAR;
                // BOOL
                case FieldType::BOOLEAN: return FixDictionaryValidator::FieldType::BOOL;

                // INT
                case FieldType::INT: return FixDictionaryValidator::FieldType::INT;
                case FieldType::TAGNUM: return FixDictionaryValidator::FieldType::INT;
                case FieldType::SEQNUM: return FixDictionaryValidator::FieldType::INT;
                case FieldType::NUMINGROUP: return FixDictionaryValidator::FieldType::INT;
                case FieldType::LENGTH: return FixDictionaryValidator::FieldType::INT;

                // FLOAT
                case FieldType::FLOAT: return FixDictionaryValidator::FieldType::FLOAT;
                case FieldType::AMT: return FixDictionaryValidator::FieldType::FLOAT;
                case FieldType::PRICE: return FixDictionaryValidator::FieldType::FLOAT;
                case FieldType::PRICEOFFSET: return FixDictionaryValidator::FieldType::FLOAT;
                case FieldType::QTY: return FixDictionaryValidator::FieldType::FLOAT;
                case FieldType::QUANTITY: return FixDictionaryValidator::FieldType::FLOAT;
                case FieldType::PERCENTAGE: return FixDictionaryValidator::FieldType::FLOAT;

                // TIMESTAMPS
                case FieldType::UTCTIMESTAMP: return FixDictionaryValidator::FieldType::TIMESTAMP;
                case FieldType::TZTIMESTAMP: return FixDictionaryValidator::FieldType::TIMESTAMP;

                // TIME ONLY
                case FieldType::UTCTIMEONLY: return FixDictionaryValidator::FieldType::TIME_ONLY;
                case FieldType::TZTIMEONLY: return FixDictionaryValidator::FieldType::TIME_ONLY;
                case FieldType::UTCTIME: return FixDictionaryValidator::FieldType::TIME_ONLY;
                case FieldType::TIME: return FixDictionaryValidator::FieldType::TIME_ONLY;
                case FieldType::LOCALMKTTIME: return FixDictionaryValidator::FieldType::TIME_ONLY;

                // DATE ONLY
                case FieldType::UTCDATEONLY: return FixDictionaryValidator::FieldType::DATE_ONLY;
                case FieldType::UTCDATE: return FixDictionaryValidator::FieldType::DATE_ONLY;
                case FieldType::DATE: return FixDictionaryValidator::FieldType::DATE_ONLY;
                case FieldType::LOCALMKTDATE: return FixDictionaryValidator::FieldType::DATE_ONLY;

                // DATE OTHER
                case FieldType::MONTHYEAR: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::DAYOFMONTH: return FixDictionaryValidator::FieldType::INT;

                // BYTE ARRAY
                case FieldType::DATA: return FixDictionaryValidator::FieldType::DATA;
                case FieldType::XMLDATA: return FixDictionaryValidator::FieldType::DATA;

                // STRING
                case FieldType::STRING: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::MULTIPLESTRINGVALUE: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::MULTIPLEVALUESTRING: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::MULTIPLEVALUECHAR: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::MULTIPLECHARVALUE: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::COUNTRY: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::CURRENCY: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::EXCHANGE: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::LANGUAGE: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::RESERVED100PLUS: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::RESERVED1000PLUS: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::RESERVED4000PLUS: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::PATTERN: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::TENOR: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::XID: return FixDictionaryValidator::FieldType::STRING;
                case FieldType::XIDREF: return FixDictionaryValidator::FieldType::STRING;

                //
                case FieldType::NONE: return FixDictionaryValidator::FieldType::NONE;
                default: return FixDictionaryValidator::FieldType::NONE;
            }
        }
        #endif

        void set_last_error_tag(uint32_t tag)
        {
            m_last_error_tag = tag;
        }

        FixSession(const FixSession& other) = delete;
        FixSession& operator= (const FixSession& other) = delete;
        FixSession(FixSession&& other) = delete;
        FixSession& operator=(FixSession&& other) = delete;
};

} // namespace