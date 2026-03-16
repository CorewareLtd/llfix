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
#include <cstdint>
#include <string>
#include <sstream>
#include <cstdio>

#include "core/utilities/configuration.h"
#include "core/utilities/filesystem_utilities.h"
#include "core/cpu/simd_capabilities.h"
#include "core/os/vdso.h"

#include "electronic_trading/session/incoming_throttler_action.h"

#include "fix_protocol_version.h"

namespace llfix
{

struct FixSessionSettings
{
    ////////////////////////////////////////////////////////////
    // GENERAL
    uint64_t heartbeat_interval_in_nanoseconds = 30'000'000'000;           // Applies to FixClient only
    bool enable_simd_avx2 = false;
    std::string str_timestamp_subseconds_precision = "NANO";
    VDSO::SubsecondPrecision timestamp_subseconds_precision = VDSO::SubsecondPrecision::NANOSECONDS;
    ////////////////////////////////////////////////////////////
    // HEADERS
    std::string begin_string;
    std::string sender_comp_id;
    std::string target_comp_id;
    std::string additional_static_header_tags;
    bool include_last_processed_seqnum_in_header = false;                 // Applies when FIX version >= 4.2
    ////////////////////////////////////////////////////////////
    // LOGONS
    std::string default_app_ver_id;                                        // Applies to FixClient only + applies when FIX version >= 5.0
    std::string logon_username;                                            // Applies to FixClient only
    std::string logon_password;                                            // Applies to FixClient only
    std::string logon_message_new_password;                                // Applies to FixClient only
    bool logon_reset_sequence_numbers=false;                               // Applies to FixClient only
    int logon_timeout_seconds = 5;                                         // Applies to FixClient only
    int logout_timeout_seconds = 5;                                        // Applies to FixClient only
    bool logon_include_next_expected_seq_no = false;                       // Applies to FixClient only + applies when FIX version >= 4.4
    ////////////////////////////////////////////////////////////
    // VALIDATIONS
    bool validations_enabled = true;
    bool validate_repeating_groups = false;
    std::size_t max_allowed_message_age_seconds = 0;                        // Value 0 disables staleness check
    ////////////////////////////////////////////////////////////
    // INCOMING RESEND REQUESTS
    bool replay_messages_on_incoming_resend_request = false;                // If false we send a gap fill msg otherwise we replay the old messages
    std::size_t replay_message_cache_initial_size=10240;
    std::size_t max_resend_range=10000;
    bool include_t97_during_resends = false;
    ////////////////////////////////////////////////////////////
    // OUTGOING RESEND REQUESTS
    int outgoing_resend_request_expire_in_secs = 30;
    ////////////////////////////////////////////////////////////
    // OUTGOING TEST REQUESTS
    int outgoing_test_request_interval_multiplier = 2;                     // Will multiply heartbeat_interval_in_nanoseconds to calculate outgoing_test_request_interval_in_nanoseconds
    ////////////////////////////////////////////////////////////
    // THROTTLER
    uint64_t throttle_window_in_milliseconds = 1'000;
    std::size_t throttle_limit = 0;                                            // Value 0 disables throttling

    std::string str_throttle_action = "WAIT";                                  // Applies to FixServer only
    IncomingThrottlerAction throttle_action = IncomingThrottlerAction::WAIT;   // Applies to FixServer only
    std::string throttler_reject_message = "Message rate limit exceeded";      // Applies to FixServer only
    ////////////////////////////////////////////////////////////
    // SERIALISATION
    std::string sequence_store_file_path = "sequence.store";
    std::string incoming_message_serialisation_path = "messages_incoming";
    std::string outgoing_message_serialisation_path = "messages_outgoing";
    std::size_t max_serialised_file_size = 67108864;                            // 64 MB, value 0 disables
    ////////////////////////////////////////////////////////////
    // SCHEDULE
    std::string schedule_week_days;
    int start_hour_utc=-1;
    int start_minute_utc=-1;
    int end_hour_utc=-1;
    int end_minute_utc=-1;
    ////////////////////////////////////////////////////////////
    // DICTIONARY
    #ifdef LLFIX_ENABLE_DICTIONARY
    mutable std::string application_dictionary_path;
    mutable std::string transport_dictionary_path;                              // Applies when FIX version >= 5
    bool dictionary_validations_enabled = true;
    #endif
    ////////////////////////////////////////////////////////////
    int protocol_version = 0;                                                   // Derived from begin_string
    uint64_t outgoing_test_request_interval_in_nanoseconds = 60'000'000'000;    // Derived from heartbeat_interval_in_nanoseconds & outgoing_test_request_interval_multiplier

    mutable bool is_server = true;                                              // Set by FixClient/FixServer
    mutable std::size_t tx_encode_buffer_capacity = 212992;                     // Set by FixClient/FixServer

    std::string config_load_error;                                              // Set by load method
    mutable std::string validation_error;                                       // Set by validate method

    void set_heartbeat_interval_in_nanoseconds(int heartbeat_interval_seconds)
    {
        heartbeat_interval_in_nanoseconds = static_cast<uint64_t>(heartbeat_interval_seconds) * static_cast<uint64_t>(1'000'000'000);
    }

    bool load_from_config_file(const std::string& config_file_path, const std::string& config_group_name)
    {
        Configuration config;

        if (config.load_from_file(config_file_path, config_load_error) == false)
        {
            return false;
        }

        if (config.does_group_exist(config_group_name) == false)
        {
            config_load_error = config_group_name + " does not exist";
            return false;
        }

        // CHECK AGAINST INVALID CONFIGS
        if (config.validate_loaded_configs({
                                             // GENERAL
                                             "heartbeat_interval_seconds", "timestamp_subseconds_precision", "enable_simd_avx2",
                                             // HEADERS
                                             "begin_string", "sender_comp_id", "target_comp_id", "additional_static_header_tags", "include_last_processed_seqnum_in_header",
                                             // LOGONS
                                             "default_app_ver_id", "logon_username", "logon_password", "logon_message_new_password", "logon_reset_sequence_numbers", "logon_timeout_seconds", "logout_timeout_seconds", "logon_include_next_expected_seq_no",
                                             // VALIDATIONS
                                             "validations_enabled", "validate_repeating_groups", "max_allowed_message_age_seconds",
                                             // INCOMING RESEND REQUESTS
                                             "replay_messages_on_incoming_resend_request", "replay_message_cache_initial_size", "max_resend_range", "include_t97_during_resends",
                                             // OUTGOING RESEND REQUESTS
                                             "outgoing_resend_request_expire_in_secs",
                                             // OUTGOING TEST REQUESTS
                                             "outgoing_test_request_interval_multiplier",
                                             // THROTTLER
                                             "throttle_window_in_milliseconds", "throttle_limit", "throttle_action", "throttler_reject_message",
                                             // SERIALISATION
                                             "sequence_store_file_path", "incoming_message_serialisation_path", "outgoing_message_serialisation_path", "max_serialised_file_size",
                                             // SCHEDULE
                                             "schedule_week_days", "start_hour_utc", "start_minute_utc", "end_hour_utc", "end_minute_utc",
                                             // DICTIONARY
                                             "application_dictionary_path", "transport_dictionary_path", "dictionary_validations_enabled"
                                            }, config_load_error, config_group_name) == false)
        {
            return false;
        }

        // GENERAL
        auto heartbeat_interval_seconds = config.get_int_value("heartbeat_interval_seconds", 30, config_group_name);
        set_heartbeat_interval_in_nanoseconds(heartbeat_interval_seconds);

        str_timestamp_subseconds_precision= config.get_string_value("timestamp_subseconds_precision", "NANO", config_group_name);
        timestamp_subseconds_precision = VDSO::string_to_subsecond_precision(str_timestamp_subseconds_precision);

        enable_simd_avx2 = config.get_bool_value("enable_simd_avx2", false, config_group_name);

        // HEADERS
        begin_string = config.get_string_value("begin_string", "", config_group_name);

        sender_comp_id = config.get_string_value("sender_comp_id", "", config_group_name);
        target_comp_id = config.get_string_value("target_comp_id","", config_group_name);
        additional_static_header_tags = config.get_string_value("additional_static_header_tags", "", config_group_name);
        include_last_processed_seqnum_in_header = config.get_bool_value("include_last_processed_seqnum_in_header", false, config_group_name);

        // LOGONS
        default_app_ver_id = config.get_string_value("default_app_ver_id", "", config_group_name);

        logon_username = config.get_string_value("logon_username","", config_group_name);
        logon_password = config.get_string_value("logon_password","", config_group_name);
        logon_message_new_password = config.get_string_value("logon_message_new_password", "", config_group_name);
        logon_reset_sequence_numbers = config.get_bool_value("logon_reset_sequence_numbers", false, config_group_name);
        logon_include_next_expected_seq_no = config.get_bool_value("logon_include_next_expected_seq_no", false, config_group_name);

        logon_timeout_seconds = config.get_int_value("logon_timeout_seconds", 5, config_group_name);
        logout_timeout_seconds = config.get_int_value("logout_timeout_seconds", 5, config_group_name);

        // VALIDATIONS
        validations_enabled = config.get_bool_value("validations_enabled", true, config_group_name);
        validate_repeating_groups = config.get_bool_value("validate_repeating_groups", false, config_group_name);
        max_allowed_message_age_seconds = config.get_int_value("max_allowed_message_age_seconds", 0, config_group_name);

        // INCOMING RESEND REQUESTS
        replay_messages_on_incoming_resend_request = config.get_bool_value("replay_messages_on_incoming_resend_request", false, config_group_name);
        replay_message_cache_initial_size = config.get_int_value("replay_message_cache_initial_size", 10240, config_group_name);
        max_resend_range = config.get_int_value("max_resend_range", 10000, config_group_name);
        include_t97_during_resends = config.get_bool_value("include_t97_during_resends", false, config_group_name);

        // OUTGOING RESEND REQUESTS
        outgoing_resend_request_expire_in_secs = config.get_int_value("outgoing_resend_request_expire_in_secs", 30, config_group_name);

        // OUTGOING TEST REQUESTS
        outgoing_test_request_interval_multiplier = config.get_int_value("outgoing_test_request_interval_multiplier", 2, config_group_name);

        // THROTTLER
        throttle_window_in_milliseconds = config.get_int_value("throttle_window_in_milliseconds", 1000, config_group_name);
        throttle_limit = config.get_int_value("throttle_limit", 0, config_group_name);

        str_throttle_action = config.get_string_value("throttle_action", "WAIT", config_group_name);
        throttle_action = string_to_incoming_throttler_action(str_throttle_action);

        throttler_reject_message = config.get_string_value("throttler_reject_message", "Message rate limit exceeded", config_group_name);

        // SERIALISATION
        sequence_store_file_path = config.get_string_value("sequence_store_file_path", "sequence.store", config_group_name);
        sequence_store_file_path = FileSystemUtilities::convert_relative_path_to_absolute_path(sequence_store_file_path);
        sequence_store_file_path = FileSystemUtilities::normalise_path(sequence_store_file_path);

        incoming_message_serialisation_path = config.get_string_value("incoming_message_serialisation_path", "messages_incoming", config_group_name);
        incoming_message_serialisation_path = FileSystemUtilities::convert_relative_path_to_absolute_path(incoming_message_serialisation_path);
        incoming_message_serialisation_path = FileSystemUtilities::normalise_path(incoming_message_serialisation_path);

        outgoing_message_serialisation_path = config.get_string_value("outgoing_message_serialisation_path", "messages_outgoing", config_group_name);
        outgoing_message_serialisation_path = FileSystemUtilities::convert_relative_path_to_absolute_path(outgoing_message_serialisation_path);
        outgoing_message_serialisation_path = FileSystemUtilities::normalise_path(outgoing_message_serialisation_path);

        max_serialised_file_size = config.get_int_value("max_serialised_file_size", 67108864, config_group_name);

        // SCHEDULE
        schedule_week_days = config.get_string_value("schedule_week_days", "", config_group_name);
        start_hour_utc = config.get_int_value("start_hour_utc", -1, config_group_name);
        start_minute_utc = config.get_int_value("start_minute_utc", -1, config_group_name);
        end_hour_utc = config.get_int_value("end_hour_utc", -1, config_group_name);
        end_minute_utc = config.get_int_value("end_minute_utc", -1, config_group_name);

        // DICTIONARY
        #ifdef LLFIX_ENABLE_DICTIONARY
        application_dictionary_path = config.get_string_value("application_dictionary_path", "", config_group_name);
        transport_dictionary_path = config.get_string_value("transport_dictionary_path", "", config_group_name);
        dictionary_validations_enabled = config.get_bool_value("dictionary_validations_enabled", true, config_group_name);
        #endif

        initialise_derived_settings();

        return true;
    }

    void initialise_derived_settings()
    {
        protocol_version = begin_string_to_fix_protocol_version(begin_string);
        outgoing_test_request_interval_in_nanoseconds = static_cast<uint64_t>(heartbeat_interval_in_nanoseconds * outgoing_test_request_interval_multiplier);
    }

    bool validate() const
    {
        ///////////////////////////////////////////////////////////////////////////////////////
        // GENERAL
        if (heartbeat_interval_in_nanoseconds == 0)
        {
            validation_error = "heartbeat_interval_seconds should be greater than zero";
            return false;
        }

        if( !(str_timestamp_subseconds_precision == "NANO" || str_timestamp_subseconds_precision == "MICRO" || str_timestamp_subseconds_precision == "MILLI" || str_timestamp_subseconds_precision == "NONE" ) )
        {
            validation_error = "timestamp_subseconds_precision should be one of : NANO,MICRO,MILLI,NONE";
            return false;
        }

        if(enable_simd_avx2)
        {
            if(llfix::SIMDCapabilities::instance().supports_simd_avx2() == false)
            {
                validation_error = "You specified SIMD AVX2, however this host does not support it.";
                return false;
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // HEADERS
        if(begin_string.length() == 0)
        {
            validation_error = "begin_string length should be greater than zero";
            return false;
        }
        else
        {
            if(begin_string != "FIXT.1.1" && begin_string != "FIX.4.4" && begin_string != "FIX.4.3" && begin_string != "FIX.4.2" && begin_string != "FIX.4.1" && begin_string != "FIX.4.0")
            {
                validation_error = "begin_string should be one of : FIXT.1.1, FIX.4.4, FIX.4.3, FIX.4.2, FIX.4.1, FIX.4.0";
                return false;
            }
        }

        if(sender_comp_id.length() == 0)
        {
            validation_error = "sender_comp_id length should be greater than zero";
            return false;
        }

        if(target_comp_id.length() == 0)
        {
            validation_error = "target_comp_id length should be greater than zero";
            return false;
        }

        if(include_last_processed_seqnum_in_header)
        {
            if(protocol_version < FixProtocolVersion::FIX42)
            {
                fprintf(stderr, "WARNING : If you are using a FIX version earlier than 4.2, specifying include_last_processed_seqnum_in_header config may lead to logon rejects\n");
            }
        }

        if(additional_static_header_tags.length()>0)
        {
            if(additional_static_header_tags.length() < 3)
            {
                validation_error = "Please provide additional_static_header_tags in the following comma separated format : <tag>=<value>,<tag>=<value>,...";
                return false;
            }

            if (additional_static_header_tags.find('=') == std::string::npos)
            {
                validation_error = "Please provide additional_static_header_tags in the following comma separated format : <tag>=<value>,<tag>=<value>,...";
                return false;
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // LOGON MESSAGE
        if(protocol_version>= FixProtocolVersion::FIX50)
        {
            if(default_app_ver_id.length() == 0)
            {
                validation_error = "You need to provide default_app_ver_id for FIX version 5.0 and later.";
                return false;
            }
        }
        else
        {
            if(default_app_ver_id.length() > 0)
            {
                fprintf(stderr, "WARNING : If you are using a FIX version earlier than 5, specifying default_app_ver_id config may lead to logon rejects\n");
            }
        }

        if (logon_timeout_seconds <= 0)
        {
            validation_error = "logon_timeout_seconds should be greater than zero";
            return false;
        }

        if (logout_timeout_seconds <= 0)
        {
            validation_error = "logout_timeout_seconds should be greater than zero";
            return false;
        }

        if(logon_include_next_expected_seq_no)
        {
            if(protocol_version < FixProtocolVersion::FIX44)
            {
                fprintf(stderr, "WARNING : If you are using a FIX version earlier than 4.4, specifying logon_include_next_expected_seq_no config may lead to logon rejects\n");
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // VALIDATIONS
        if( validations_enabled == false && validate_repeating_groups == true)
        {
            fprintf(stderr, "WARNING : validate_repeating_groups is set true, however it won't take effect since validations_enabled is set to false\n");
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // INCOMING RESEND REQUESTS
        if(replay_messages_on_incoming_resend_request==true)
        {
            if(replay_message_cache_initial_size == 0 )
            {
                validation_error = "replay_message_cache_initial_size should be greater than zero when replay_messages_on_incoming_resend_request is set to true";
                return false;
            }

            if(max_resend_range == 0)
            {
                validation_error = "max_resend_range should be greater than zero when replay_messages_on_incoming_resend_request is set to true";
                return false;
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // OUTGOING RESEND REQUESTS
        if(outgoing_resend_request_expire_in_secs <= 0)
        {
            validation_error = "outgoing_resend_request_expire_in_secs should be greater than zero";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // OUTGOING TEST REQUESTS
        if(outgoing_test_request_interval_multiplier <= 1)
        {
            validation_error = "outgoing_test_request_interval_multiplier should be greater than one";
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // THROTTLER
        if (throttle_limit > 0)
        {
            if (throttle_window_in_milliseconds == 0)
            {
                validation_error = "throttle_window_in_milliseconds should be greater than zero";
                return false;
            }

            if(throttle_limit>10000)
            {
                fprintf(stderr, "WARNING : Using high values for throttle_limit config can slow down throttler and will use higher memory. Used value : %zu\n", throttle_limit);
            }

            if(str_throttle_action != "WAIT" && str_throttle_action != "DISCONNECT" && str_throttle_action != "REJECT" )
            {
                validation_error = "throttle_action can only be one of : 'WAIT' 'DISCONNECT' or 'REJECT'";
                return false;
            }

            if(str_throttle_action == "REJECT")
            {
                if(throttler_reject_message.length() == 0)
                {
                    validation_error = "throttler_reject_message length should be greater than zero";
                    return false;
                }
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // SERIALISATION
        if (sequence_store_file_path.length() == 0)
        {
            validation_error = "sequence_store_file_path length should be greater than zero";
            return false;
        }

        if(max_serialised_file_size>0)
        {
            if (incoming_message_serialisation_path.length() == 0)
            {
                validation_error = "incoming_message_serialisation_path length should be greater than zero";
                return false;
            }

            if (outgoing_message_serialisation_path.length() == 0)
            {
                validation_error = "outgoing_message_serialisation_path length should be greater than zero";
                return false;
            }

            // 4096 IS VIRTUAL MEMORY PAGE SIZE ON INTEL X64 ARCHITECTURES
            if (max_serialised_file_size % 4096 != 0)
            {
                validation_error = "max_serialised_file_size should be a multiple of 4096";
                return false;
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // SCHEDULE
        if (schedule_week_days.length() > 0)
        {
            bool looking_for_delimiter{ false };
            for (const auto& ch : schedule_week_days)
            {
                if (looking_for_delimiter == false)
                {
                    if (ch < '1' || ch > '7')
                    {
                        validation_error = "schedule_week_days value should be dash ('-') separated digits from 1 to 7";
                        return false;
                    }
                }
                else
                {
                    if (ch != '-')
                    {
                        validation_error = "schedule_week_days value should be dash ('-') separated digits from 1 to 7";
                        return false;
                    }
                }

                looking_for_delimiter = !looking_for_delimiter;
            }
        }

        if (start_hour_utc < -1)
        {
            validation_error = "start_hour_utc can't take negative values except -1";
            return false;
        }

        if(start_hour_utc>=0)
        {
            if( start_hour_utc>23)
            {
                validation_error = "start_hour_utc can't be greater than 23";
                return false;
            }
        }

        if (end_hour_utc < -1)
        {
            validation_error = "end_hour_utc can't take negative values except -1";
            return false;
        }

        if(end_hour_utc>=0)
        {
            if( end_hour_utc>23)
            {
                validation_error = "end_hour_utc can't be greater than 23";
                return false;
            }
        }

        if (start_minute_utc < -1)
        {
            validation_error = "start_minute_utc can't take negative values except -1";
            return false;
        }

        if(start_minute_utc>=0)
        {
            if( start_minute_utc>59)
            {
                validation_error = "start_minute_utc can't be greater than 59";
                return false;
            }
        }

        if (end_minute_utc < -1)
        {
            validation_error = "end_minute_utc can't take negative values except -1";
            return false;
        }

        if(end_minute_utc>=0)
        {
            if( end_minute_utc>59)
            {
                validation_error = "end_minute_utc can't be greater than 59";
                return false;
            }
        }

        if(start_hour_utc != -1 || end_hour_utc != -1 || start_minute_utc != -1 || end_minute_utc != -1) // If any specified
        {
            if(start_hour_utc == -1 || end_hour_utc == -1 || start_minute_utc == -1 || end_minute_utc == -1)
            {
                validation_error = "You need to set all schedule time configs (start_hour_utc, end_hour_utc, start_minute_utc, end_minute_utc) at once ";
                return false;
            }

            auto start_minutes = start_hour_utc*60+start_minute_utc;
            auto end_minutes = end_hour_utc*60+end_minute_utc;

            if(start_minutes >= end_minutes)
            {
                validation_error = "Schedule start time should be before schedule end time";
                return false;
            }
        }
        ///////////////////////////////////////////////////////////////////////////////////////
        // DICTIONARY
        #ifdef LLFIX_ENABLE_DICTIONARY
        if(application_dictionary_path.length()>0)
        {
            application_dictionary_path = FileSystemUtilities::convert_relative_path_to_absolute_path(application_dictionary_path);
            application_dictionary_path = FileSystemUtilities::normalise_path(application_dictionary_path);

            if(FileSystemUtilities::does_file_exist(application_dictionary_path) == false)
            {
                validation_error = "Specified application_dictionary_path does not exist";
                return false;
            }
        }

        if(transport_dictionary_path.length()>0)
        {
            if(protocol_version< FixProtocolVersion::FIX50)
            {
                fprintf(stderr, "WARNING : Transport dictionaries are for FIX 5 or later versions.\n");
            }

            transport_dictionary_path = FileSystemUtilities::convert_relative_path_to_absolute_path(transport_dictionary_path);
            transport_dictionary_path = FileSystemUtilities::normalise_path(transport_dictionary_path);

            if(FileSystemUtilities::does_file_exist(transport_dictionary_path) == false)
            {
                validation_error = "Specified transport_dictionary_path does not exist";
                return false;
            }

            if(application_dictionary_path.length() == 0)
            {
                validation_error = "If transport_dictionary_path specified, also application_dictionary_path should be specified";
                return false;
            }
        }

        if(protocol_version>= FixProtocolVersion::FIX50)
        {
            if(application_dictionary_path.length() > 0 && transport_dictionary_path.length() == 0)
            {
                fprintf(stderr, "WARNING : If you are using a FIX 5 or later version, you shall also specify a transport dictionary\n");
            }
        }
        #endif

        return true;
    }

    std::string to_string(const std::string& delimiter = "\n") const
    {
        std::stringstream ret;

        auto inject_category = [&](const std::string& category_name)
            {
                if (delimiter == ",")
                {
                    ret << "-" << category_name << "=" << delimiter;
                }
            };
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // DICTIONARY
        #ifdef LLFIX_ENABLE_DICTIONARY
        inject_category("DICTIONARY");
        ret << "application_dictionary_path=" << application_dictionary_path << delimiter;
        ret << "transport_dictionary_path=" << transport_dictionary_path << delimiter;
        ret << "dictionary_validations_enabled=" << std::boolalpha << dictionary_validations_enabled << delimiter;
        #endif

        // HEADERS
        inject_category("HEADERS");
        ret << "begin_string=" << begin_string << delimiter;
        ret << "sender_comp_id=" << sender_comp_id << delimiter;
        ret << "target_comp_id=" << target_comp_id << delimiter;
        ret << "additional_static_header_tags=" << additional_static_header_tags << delimiter;
        ret << "include_last_processed_seqnum_in_header=" << std::boolalpha << include_last_processed_seqnum_in_header << delimiter;

        // LOGONS
        inject_category("LOGONS");
        ret << "default_app_ver_id=" << default_app_ver_id << delimiter;
        ret << "logout_timeout_seconds=" << logout_timeout_seconds << delimiter;

        if (is_server == false)
        {
            ret << "logon_timeout_seconds=" << logon_timeout_seconds << delimiter;
            ret << "logon_username=" << logon_username << delimiter;
            ret << "logon_password=" << logon_password << delimiter;
            ret << "logon_message_new_password=" << logon_message_new_password << delimiter;
            ret << "logon_reset_sequence_numbers=" << std::boolalpha << logon_reset_sequence_numbers << delimiter;
            ret << "logon_include_next_expected_seq_no=" << std::boolalpha << logon_include_next_expected_seq_no << delimiter;
        }

        // GENERAL
        inject_category("GENERAL");
        if (is_server == false)
            ret << "heartbeat_interval_seconds=" << heartbeat_interval_in_nanoseconds / 1'000'000'000 << delimiter;

        ret << "timestamp_subseconds_precision=" << str_timestamp_subseconds_precision << delimiter;
        ret << "enable_simd_avx2=" << std::boolalpha << enable_simd_avx2 << delimiter;

        // INCOMING RESEND REQUESTS
        inject_category("INCOMING RESEND REQUESTS");
        ret << "replay_messages_on_incoming_resend_request=" << std::boolalpha << replay_messages_on_incoming_resend_request << delimiter;
        ret << "replay_message_cache_initial_size=" << replay_message_cache_initial_size << delimiter;
        ret << "max_resend_range=" << max_resend_range << delimiter;
        ret << "include_t97_during_resends=" << std::boolalpha << include_t97_during_resends << delimiter;

        // OUTGOING RESEND REQUESTS
        inject_category("OUTGOING RESEND REQUESTS");
        ret << "outgoing_resend_request_expire_in_secs=" << outgoing_resend_request_expire_in_secs << delimiter;

        // OUTGOING TEST REQUESTS
        inject_category("OUTGOING TEST REQUESTS");
        ret << "outgoing_test_request_interval_multiplier=" << outgoing_test_request_interval_multiplier << delimiter;
        if (is_server == false)
            ret << "outgoing_test_request_interval_in_nanoseconds=" << outgoing_test_request_interval_in_nanoseconds << delimiter;

        // THROTTLER
        inject_category("THROTTLER");
        ret << "throttle_window_in_milliseconds=" << throttle_window_in_milliseconds << delimiter;
        ret << "throttle_limit=" << throttle_limit << delimiter;

        if (is_server == true)
        {
            ret << "throttle_action=" << str_throttle_action << delimiter;
            ret << "throttler_reject_message=" << throttler_reject_message << delimiter;
        }

        // VALIDATIONS
        inject_category("VALIDATIONS");
        ret << "validations_enabled=" << std::boolalpha << validations_enabled << delimiter;
        ret << "validate_repeating_groups=" << std::boolalpha << validate_repeating_groups << delimiter;
        ret << "max_allowed_message_age_seconds=" << max_allowed_message_age_seconds << delimiter;

        // SERIALISATION
        inject_category("SERIALISATION");
        ret << "sequence_store_file_path=" << sequence_store_file_path << delimiter;
        ret << "incoming_message_serialisation_path=" << incoming_message_serialisation_path << delimiter;
        ret << "outgoing_message_serialisation_path=" << outgoing_message_serialisation_path << delimiter;
        ret << "max_serialised_file_size=" << max_serialised_file_size << delimiter;

        // SCHEDULE
        inject_category("SCHEDULE");
        ret << "schedule_week_days=" << schedule_week_days << delimiter;
        ret << "start_hour_utc=" << start_hour_utc << delimiter;
        ret << "start_minute_utc=" << start_minute_utc << delimiter;
        ret << "end_hour_utc=" << end_hour_utc << delimiter;
        ret << "end_minute_utc=" << end_minute_utc << delimiter;
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        return ret.str();
    }
};

} // namespace