///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#ifndef LLFIX_UNIT_TEST
#define LLFIX_UNIT_TEST
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <llfix/common.h>
#include "../unit_test.h"

#include <iostream>
using namespace std;

#include <cstdlib>
#include <cstring>

#include <cstdint>
#include <cstddef>

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>

#include "validators.h"

#include <llfix/core/compiler/unused.h>
#include <llfix/core/cpu/simd_capabilities.h>

#include <llfix/fix_string.h>
#include <llfix/fix_utilities.h>
#include <llfix/fix_string_view.h>
#include <llfix/incoming_fix_message.h>
#include <llfix/outgoing_fix_message.h>

#include <llfix/electronic_trading/common/fixed_point.h>
#include <llfix/electronic_trading/common/message_serialiser.h>
#include <llfix/electronic_trading/session/sequence_store.h>

#include <llfix/fix_session_settings.h>
#include <llfix/fix_session.h>

//#define PRINT_MESSAGES

using namespace llfix;

FixStringView* get_fix_string_view(const char* input);
std::size_t get_string_count(const std::string& content, std::string needle);

int main(int argc, char* argv[])
{
    UnitTest unit_test;

    //////////////////////////////////////////////////////////
    // OUTGOING
    {
        FixSession dummy_session;
        dummy_session.set_begin_string("FIXT.1.1");
        dummy_session.set_compid("SNDR");
        dummy_session.set_target_compid("RCVR");

        if(llfix::SIMDCapabilities::instance().supports_simd_avx2())
        {
            dummy_session.set_enable_simd_avx2(true);
        }

        static constexpr std::size_t ENCODE_BUFFER_SIZE = 4096;
        char encode_buffer[ENCODE_BUFFER_SIZE];
        std::size_t encoded_length{ 0 };

        OutgoingFixMessage msg;

        if (msg.initialise(dummy_session.settings(), dummy_session.get_sequence_store()) == false)
        {
            std::cout << "OutgoingFixMessage failed\n";
            return -1;
        }

        msg.set_additional_static_header_tag(50, "SENDER_SUB_ID");
        msg.set_additional_static_header_tag(57, "TARGET_SUB_ID");

        msg.set_msg_type("BA");

        msg.set_timestamp_tag(60);

        msg.set_tag(453, 2);
        msg.set_tag(448, "PARTY1");
        msg.set_tag(447, 'D');
        msg.set_tag(452, 1);
        msg.set_tag(448, "PARTY2");
        msg.set_tag(447, 'D');
        msg.set_tag(452, 3);

        /* Unsupported type - should not compile

            class Foo {};
            Foo f;
            msg.set_tag(455, f);
        */

        msg.set_tag(21, 3.4f, 1);
        msg.set_tag(22, 3.5, 1);
        //msg.set_body_tag(23, (uint16_t) 230); // triggers assertion -> unsupported type as expected
        msg.set_tag(24, (uint32_t)240);
        msg.set_tag(25, (uint64_t)250);
        msg.set_tag(26, (int)260);
        msg.set_tag(27, 'x');
        msg.set_tag(28, "str");
        msg.set_tag(29, std::string("twentynine"));

        msg.set_tag(30, std::string("long_string_long_string_long_string_long_string_long_string_long_string_long_string"));

        msg.set_tag(31, true);
        msg.set_tag(32, false);

        msg.set_tag(34, (int)-1);

        std::string_view str_view = "ABC";
        msg.set_tag(336, str_view);

        FixedPoint fp;
        fp.set_raw_value(10000);
        fp.set_decimal_points(3);
        msg.set_tag(33, fp);

        #ifdef PRINT_MESSAGES
        std::cout << "Before encoding =>\n";

        for (auto it : msg)
        {
            std::cout << "tag = " << std::to_string(it.tag) << " val = " << it.value->c_str() << "\n";
        }
        std::cout << "\n";


        std::string msg_as_string = msg.to_string();

        std::cout << "msg to string : " << msg_as_string << "\n\n";
        #endif

        msg.encode(encode_buffer, ENCODE_BUFFER_SIZE, 1, encoded_length);

        unit_test.test_equals(validate_message(encode_buffer, encoded_length), true, "outgoing fix message", "encoding - message validation");
        unit_test.test_equals(validate_body_length(encode_buffer, encoded_length), true, "outgoing fix message", "encoding - body length");
        unit_test.test_equals(validate_checksum(encode_buffer, encoded_length), true, "outgoing fix message", "encoding - checksum");

        for(std::size_t i=0; i< encoded_length; i++)
        {
            if (encode_buffer[i] == (char)(1))
            {
                encode_buffer[i] = '|';
            }
        }

        encode_buffer[encoded_length] = '\0';

        #ifdef PRINT_MESSAGES
        std::cout << "After encoding =>\n";
        std::cout << encode_buffer << "\n";
        std::cout << "\n";
        #endif
    }

    //////////////////////////////////////////////////////////
    // OUTGOING SET MSG TYPE
    {
        FixSession dummy_session;
        dummy_session.set_begin_string("FIXT.1.1");
        dummy_session.set_compid("SNDR");
        dummy_session.set_target_compid("RCVR");

        OutgoingFixMessage msg;

        if (msg.initialise(dummy_session.settings(), dummy_session.get_sequence_store()) == false)
        {
            std::cout << "OutgoingFixMessage failed\n";
            return -1;
        }

        msg.set_additional_static_header_tag(50, "SENDER_SUB_ID");
        msg.set_additional_static_header_tag(57, "TARGET_SUB_ID");

        msg.set_msg_type("a");
        unit_test.test_equals(msg.get_msg_type_length(), 1, "outgoing fix message", "set_msg_type length 1");

        msg.set_msg_type("BA");
        unit_test.test_equals(msg.get_msg_type_length(), 2, "outgoing fix message", "set_msg_type length 2");

        msg.set_msg_type("XLR");
        unit_test.test_equals(msg.get_msg_type_length(), 3, "outgoing fix message", "set_msg_type length 3");

        msg.set_msg_type("XLRJ");
        unit_test.test_equals(msg.get_msg_type_length(), 4, "outgoing fix message", "set_msg_type length 4");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // OUTGOING - LOAD FROM BUFFER ( USED IN MSG RESENDS FOR INCOMING 35=2 REQUESTS )
    {
        FixSession dummy_session;
        dummy_session.set_begin_string("FIXT.1.1");
        dummy_session.set_compid("SNDR");
        dummy_session.set_target_compid("RCVR");

        if(llfix::SIMDCapabilities::instance().supports_simd_avx2())
        {
            dummy_session.set_enable_simd_avx2(true);
        }

        static constexpr std::size_t ENCODE_BUFFER_SIZE = 4096;
        char encode_buffer[ENCODE_BUFFER_SIZE];
        std::size_t encoded_length{ 0 };

        OutgoingFixMessage msg;

        if (msg.initialise(dummy_session.settings(), dummy_session.get_sequence_store()) == false)
        {
            std::cout << "OutgoingFixMessage failed\n";
            return -1;
        }

        msg.set_additional_static_header_tag(50, "SENDER_SUB_ID");
        msg.set_additional_static_header_tag(57, "TARGET_SUB_ID");

        std::string orig_message = "8=FIXT.1.1|9=298|35=D|34=4012|49=CLIENT1|52=20250706-06:21:46.630896100|56=EXECUTOR|50=SNDR_SUB|57=SRVR_SUB|11=4010|1=abcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefgh|55=NOKIA.HE|54=1|38=1|44=10000|40=2|59=0|453=2|448=PARTY1|447=D|452=1|448=PARTY2|447=D|452=3|60=20250706-06:21:46.630896100|10=153|";

        for (std::size_t i = 0; i < orig_message.length(); i++)
        {
            if (orig_message[i] == '|')
            {
                orig_message[i] = (char)(1);
            }
        }

        msg.load_from_buffer(orig_message.data(), orig_message.length());

        msg.encode(encode_buffer, ENCODE_BUFFER_SIZE, 1, encoded_length);

        unit_test.test_equals(validate_message(encode_buffer, encoded_length), true, "outgoing fix message", "encoding - message");
        unit_test.test_equals(validate_body_length(encode_buffer, encoded_length), true, "outgoing fix message - resends", "encoding - body length");
        unit_test.test_equals(validate_checksum(encode_buffer, encoded_length), true, "outgoing fix message - resends", "encoding - checksum");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // OUTGOING - MESSAGE REPLAYS
    {
        const std::string serialisation_path = "./messages_serialisations/";
        const std::size_t serialisation_max_file_size = 4096;
        const std::size_t record_count = 80;

        MessageSerialiser<llfix::FixMessageSequenceNumberExtractor> serialiser;

        if (serialiser.initialise(serialisation_path, serialisation_max_file_size, true, 6553600) == false)
        {
            std::cout << "Failed to open serialised files.\n";
            return -1;
        }

        FixSessionSettings session_settings;
        session_settings.begin_string = "FIXT.1.1";
        session_settings.sender_comp_id = "SENDER";
        session_settings.target_comp_id = "TARGET";

        SequenceStore seq_store;

        const std::string sequence_store_path = "sequence.store";
        llfix::FileSystemUtilities::delete_file_if_exists(sequence_store_path);

        if (seq_store.open(sequence_store_path) == false)
        {
            std::cout << "Failed to open sequence store.\n";
            return -1;
        }

        OutgoingFixMessage outgoing_message;

        if (outgoing_message.initialise(&session_settings, &seq_store) == false)
        {
            std::cout << "Failed to initialise outgoing message.\n";
            return -1;
        }

        unit_test.test_equals(serialiser.get_message_record_count(), record_count, "messsage serialiser", "initialisation");

        std::string output_content;

        for (std::size_t i = 1; i <= record_count; ++i)
        {
            auto current_seq_no = static_cast<uint32_t>(i);
            unit_test.test_equals(serialiser.has_message_in_memory(current_seq_no), true, "messsage serialiser", "has_message_in_memory" + std::to_string(i));

            std::size_t current_message_length = 0;
            char buffer[4096];
            serialiser.read_message(current_seq_no, buffer, sizeof(buffer), current_message_length);

            outgoing_message.load_from_buffer(buffer, current_message_length);
            char encode_buffer[4096];
            std::size_t encoded_length{ 0 };
            outgoing_message.encode(encode_buffer, sizeof(encode_buffer), current_seq_no, encoded_length);
            output_content += FixUtilities::fix_to_human_readible(encode_buffer, encoded_length) + "\n";
        }

        unit_test.test_equals(get_string_count(output_content, "122="), record_count, "message replay", "orig sending time");
        unit_test.test_equals(get_string_count(output_content, "35=4"), 5, "message replay", "non retransmittable admin messages");
        unit_test.test_equals(get_string_count(output_content, "35=8"), record_count - 5, "message replay", "retransmittable messages");
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////
    // INCOMING
    {
        IncomingFixMessage msg;

        if (msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        {
            msg.set_tag(55, get_fix_string_view("55"));

            msg.set_tag(58, get_fix_string_view("55.1234"));

            msg.set_tag(88, get_fix_string_view("N"));
            msg.set_tag(89, get_fix_string_view("Y"));

            auto str_vw = msg.get_tag_value(55);
            auto retrieved = str_vw->to_string();
            unit_test.test_equals(retrieved.c_str(), "55", "incoming fix message", "get_tag_value");

            auto retrived_string = msg.get_tag_value_as<std::string>(55);
            unit_test.test_equals(retrived_string.c_str(), "55", "incoming fix message", "get_tag_value_as<std::string>");

            /* Unsupported type - should not compile
                class Foo {};
                msg.get_tag_value_as<Foo>(55);
                msg.get_repeating_group_tag_value_as<Foo>(55, 0);
            */

            unit_test.test_equals(msg.get_tag_value_as<int>(55), (int)(55), "incoming fix message", "get_tag_value_as<int>");
            unit_test.test_equals(msg.get_tag_value_as<uint32_t>(55), (uint32_t)(55), "incoming fix message", "get_tag_value_as<uint32_t>");
            unit_test.test_equals(msg.get_tag_value_as<uint64_t>(55), (uint64_t)(55), "incoming fix message", "get_tag_value_as<uint64_t>");
            unit_test.test_equals(msg.get_tag_value_as<char>(55), '5', "incoming fix message", "get_tag_value_as<char>");
            unit_test.test_equals(msg.get_tag_value_as<std::string>(55), std::string("55"), "incoming fix message", "get_tag_value_as<std::string>");
            unit_test.test_equals(msg.get_tag_value_as<std::string_view>(55), "55", "incoming fix message", "get_tag_value_as<std::string_view>");

            unit_test.test_equals(msg.get_tag_value_as<bool>(89), true, "incoming fix message", "get_tag_value_as<bool> positive");
            unit_test.test_equals(msg.get_tag_value_as<bool>(88), false, "incoming fix message", "get_tag_value_as<bool> negative");

            unit_test.test_equals(msg.get_tag_value_as<double>(58, 4), (double)(55.1234), "incoming fix message", "get_tag_value_as<double>");

            FixedPoint fp_test;
            fp_test.set_decimal_points(4);
            fp_test.set_raw_value(551234);
            unit_test.test_equals(msg.get_tag_value_as<FixedPoint>(58, 4).get_raw_value(), fp_test.get_raw_value(), "incoming fix message", "get_tag_value_as<FixedPoint> raw value");

            auto retrieved_int32 = msg.get_tag_value_as<uint32_t>(55);
            unit_test.test_equals(retrieved_int32, 55, "incoming fix message", "get_tag_value_as<uint32_t>");

            auto retrieved_int64 = msg.get_tag_value_as<uint64_t>(55);
            unit_test.test_equals(retrieved_int64, 55, "incoming fix message", "get_tag_value_as<uint64_t>");

            auto is_numeric = msg.is_tag_value_numeric(55);
            unit_test.test_equals(is_numeric, true, "incoming fix message", "is_tag_value_numeric");

            unit_test.test_equals(msg.has_tag(55), true, "incoming fix messag", "has_tag");

            unit_test.test_equals(msg.get_tag_value_as<bool>(88), false, "incoming fix messag", "get_tag_value_as<bool> false");
            unit_test.test_equals(msg.get_tag_value_as<bool>(89), true, "incoming fix messag", "get_tag_value_as<bool> true");
        }

        {
            char val = 'w';
            FixStringView fix_str_view;
            fix_str_view.set_buffer(&val, 1);
            msg.set_tag(66, &fix_str_view);

            auto retrieved = msg.get_tag_value_as<char>(66);
            unit_test.test_equals(retrieved, 'w', "incoming fix message", "get_tag_value_as<char>");
        }

        {
            std::string val = "77.77";
            FixStringView fix_str_view;
            fix_str_view.set_buffer(val.data(), val.length());
            msg.set_tag(77, &fix_str_view);

            auto retrieved_double = msg.get_tag_value_as<double>(77, 2);
            unit_test.test_equals(retrieved_double, 77.77, "incoming fix message", "get_tag_value_as<double>");

            auto retrieved_fixed_point = msg.get_tag_value_as<FixedPoint>(77, 4);
            unit_test.test_equals(retrieved_fixed_point.get_decimal_points(), 4, "incoming fix message", "get_tag_value_as<fixed_point> decimal_points");
            unit_test.test_equals(retrieved_fixed_point.get_raw_value(), 777700, "incoming fix message", "get_tag_value_as<fixed_point> raw_value");
        }

        {
            msg.set_repeating_group_tag(453, get_fix_string_view("3"));

            msg.set_repeating_group_tag(448, get_fix_string_view("val448-1"));
            msg.set_repeating_group_tag(447, get_fix_string_view("4471"));
            msg.set_repeating_group_tag(452, get_fix_string_view("x"));

            msg.set_repeating_group_tag(448, get_fix_string_view("val448-2"));
            msg.set_repeating_group_tag(447, get_fix_string_view("4472"));
            msg.set_repeating_group_tag(452, get_fix_string_view("y"));

            msg.set_repeating_group_tag(448, get_fix_string_view("val448-3"));
            msg.set_repeating_group_tag(447, get_fix_string_view("4473"));
            msg.set_repeating_group_tag(452, get_fix_string_view("z"));

            msg.set_repeating_group_tag(600, get_fix_string_view("3"));

            msg.set_repeating_group_tag(601, get_fix_string_view("xPARTY1"));
            msg.set_repeating_group_tag(602, get_fix_string_view("xD"));
            msg.set_repeating_group_tag(603, get_fix_string_view("x1"));

            msg.set_repeating_group_tag(601, get_fix_string_view("yPARTY2"));
            msg.set_repeating_group_tag(602, get_fix_string_view("yE"));
            msg.set_repeating_group_tag(603, get_fix_string_view("y2"));

            msg.set_repeating_group_tag(601, get_fix_string_view("PARTY3"));
            msg.set_repeating_group_tag(602, get_fix_string_view("F"));
            msg.set_repeating_group_tag(603, get_fix_string_view("3"));
        }

        {
            unit_test.test_equals(msg.has_repeating_group_tag(453), true, "incoming fix message", "has_repeating_group_tag 453");
            unit_test.test_equals(msg.has_repeating_group_tag(448), true, "incoming fix message", "has_repeating_group_tag 448");
            unit_test.test_equals(msg.has_repeating_group_tag(447), true, "incoming fix message", "has_repeating_group_tag 447");
            unit_test.test_equals(msg.has_repeating_group_tag(452), true, "incoming fix message", "has_repeating_group_tag 452");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<int>(453, 0), 3, "incoming fix message", "get_repeating_group_tag_value_as<int> 453");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<uint32_t>(453, 0), 3, "incoming fix message", "get_repeating_group_tag_value_as<uint32_t> 453");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<uint64_t>(453, 0), 3, "incoming fix message", "get_repeating_group_tag_value_as<uint64_t> 453");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(448, 0), "val448-1", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 448");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(448, 1), "val448-2", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 448");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(448, 2), "val448-3", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 448");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<uint32_t>(447, 0), 4471, "incoming fix message", "get_repeating_group_tag_value_as<uint32_t> 447");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<uint32_t>(447, 1), 4472, "incoming fix message", "get_repeating_group_tag_value_as<uint32_t> 447");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<uint32_t>(447, 2), 4473, "incoming fix message", "get_repeating_group_tag_value_as<uint32_t> 447");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<char>(452, 0), 'x', "incoming fix message", "get_repeating_group_tag_value_as<char> 452");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<char>(452, 1), 'y', "incoming fix message", "get_repeating_group_tag_value_as<char> 452");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<char>(452, 2), 'z', "incoming fix message", "get_repeating_group_tag_value_as<char> 452");
        }

        {
            unit_test.test_equals(msg.has_repeating_group_tag(600), true, "incoming fix message", "has_repeating_group_tag 600");
            unit_test.test_equals(msg.has_repeating_group_tag(601), true, "incoming fix message", "has_repeating_group_tag 601");
            unit_test.test_equals(msg.has_repeating_group_tag(602), true, "incoming fix message", "has_repeating_group_tag 602");
            unit_test.test_equals(msg.has_repeating_group_tag(603), true, "incoming fix message", "has_repeating_group_tag 603");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<uint32_t>(600, 0), 3, "incoming fix message", "get_repeating_group_tag_value_as<uint32_t> 600");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<uint64_t>(600, 0), 3, "incoming fix message", "get_repeating_group_tag_value_as<uint64_t> 600");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<int>(600, 0), 3, "incoming fix message", "get_repeating_group_tag_value_as<int> 600");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<double>(600, 0, 2), 3, "incoming fix message", "get_repeating_group_tag_value_as<double> 600");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<llfix::FixedPoint>(600, 0, 2).get_raw_value(), 300, "incoming fix message", "get_repeating_group_tag_value_as<FixedPoint> 600");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<bool>(600, 0), false, "incoming fix message", "get_repeating_group_tag_value_as<bool> 600");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(601, 0), "xPARTY1", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 601");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(601, 1), "yPARTY2", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 601");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(601, 2), "PARTY3", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 601");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(602, 0), "xD", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 602");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string_view>(602, 0), "xD", "incoming fix message", "get_repeating_group_tag_value_as<std::string_view> 602");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(602, 1), "yE", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 602");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string_view>(602, 1), "yE", "incoming fix message", "get_repeating_group_tag_value_as<std::string_view> 602");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(602, 2), "F", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 602");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(603, 0), "x1", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 603");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(603, 1), "y2", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 603");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(603, 2), "3", "incoming fix message", "get_repeating_group_tag_value_as<std::string> 603");

            unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string_view>(603, 2), "3", "incoming fix message", "get_repeating_group_tag_value_as<std::string_view> 603");
            unit_test.test_equals(msg.get_repeating_group_tag_value_as<char>(603, 2), '3', "incoming fix message", "get_repeating_group_tag_value_as<char> 603");
        }
    }

    //////////////////////////////////////////////////////////
    // INCOMING - VALUE RANGES
    {
        IncomingFixMessage msg;

        if (msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        const uint32_t test_tag = 58;

        // uint32_t
        msg.set_tag(test_tag, get_fix_string_view("0"));
        unit_test.test_equals(msg.get_tag_value_as<uint32_t>(test_tag), 0, "incoming fix message, value ranges", "uint32_t 0");

        const uint32_t uint32_max = (std::numeric_limits<uint32_t>::max)();
        std::string uint32_max_str = std::to_string(uint32_max);
        msg.set_tag(test_tag, get_fix_string_view(uint32_max_str.c_str()));
        unit_test.test_equals(msg.get_tag_value_as<uint32_t>(test_tag), uint32_max, "incoming fix message, value ranges", "uint32_t max");

        // uint64_t
        msg.set_tag(test_tag, get_fix_string_view("0"));
        unit_test.test_equals(msg.get_tag_value_as<uint64_t>(test_tag), 0, "incoming fix message, value ranges", "uint64_t 0");

        const uint64_t uint64_max = (std::numeric_limits<uint64_t>::max)();
        std::string uint64_max_str = std::to_string(uint64_max);
        msg.set_tag(test_tag, get_fix_string_view(uint64_max_str.c_str()));
        unit_test.test_equals(msg.get_tag_value_as<uint64_t>(test_tag), uint64_max, "incoming fix message, value ranges", "uint64_t max");

        // int
        msg.set_tag(test_tag, get_fix_string_view("0"));
        unit_test.test_equals(msg.get_tag_value_as<int>(test_tag), 0, "incoming fix message, value ranges", "int 0");

        const int int_min = (std::numeric_limits<int>::min)();
        const int int_max = (std::numeric_limits<int>::max)();
        std::string int_min_str = std::to_string(int_min);
        std::string int_max_str = std::to_string(int_max);
        msg.set_tag(test_tag, get_fix_string_view(int_min_str.c_str()));
        unit_test.test_equals(msg.get_tag_value_as<int>(test_tag), int_min, "incoming fix message, value ranges", "int min");
        msg.set_tag(test_tag, get_fix_string_view(int_max_str.c_str()));
        unit_test.test_equals(msg.get_tag_value_as<int>(test_tag), int_max, "incoming fix message, value ranges", "int max");

        // double
        msg.set_tag(test_tag, get_fix_string_view("0"));
        std::size_t decimal_points = 1;
        unit_test.test_equals(msg.get_tag_value_as<double>(test_tag, decimal_points), (double)0, "incoming fix message, value ranges", "double 0");

        msg.set_tag(test_tag, get_fix_string_view("0.0"));
        decimal_points = 1;
        unit_test.test_equals(msg.get_tag_value_as<double>(test_tag, decimal_points), (double)0, "incoming fix message, value ranges", "double 0.0");

        const double double_lowest = (std::numeric_limits<double>::lowest)();
        const double double_max = (std::numeric_limits<double>::max)();
        std::ostringstream double_stream;
        double_stream.setf(std::ios::fixed);
        double_stream << std::setprecision(6) << double_lowest;
        std::string double_lowest_str = double_stream.str();
        double_stream.str(std::string());
        double_stream.clear();
        double_stream << std::setprecision(6) << double_max;
        std::string double_max_str = double_stream.str();

        msg.set_tag(test_tag, get_fix_string_view(double_lowest_str.c_str()));
        decimal_points = 6;
        unit_test.test_equals(msg.get_tag_value_as<double>(test_tag, decimal_points), double_lowest, "incoming fix message, value ranges", "double lowest");

        msg.set_tag(test_tag, get_fix_string_view(double_max_str.c_str()));
        decimal_points = 6;
        unit_test.test_equals(msg.get_tag_value_as<double>(test_tag, decimal_points), double_max, "incoming fix message, value ranges", "double max");

        // llfix::FixedPoint
        msg.set_tag(test_tag, get_fix_string_view("0"));
        decimal_points = 1;
        unit_test.test_equals(msg.get_tag_value_as<llfix::FixedPoint>(test_tag, decimal_points).get_raw_value(), (uint64_t)0, "incoming fix message, value ranges", "FixedPoint 0");

        msg.set_tag(test_tag, get_fix_string_view("0.0"));
        decimal_points = 1;
        unit_test.test_equals(msg.get_tag_value_as<llfix::FixedPoint>(test_tag, decimal_points).get_raw_value(), (uint64_t)0, "incoming fix message, value ranges", "FixedPoint 0.0");

        const uint64_t fixed_point_max = (std::numeric_limits<uint64_t>::max)();
        std::string fixed_point_max_str = std::to_string(fixed_point_max);
        fixed_point_max_str.insert(fixed_point_max_str.size() - 1, ".");
        msg.set_tag(test_tag, get_fix_string_view(fixed_point_max_str.c_str()));
        decimal_points = 1;
        unit_test.test_equals(msg.get_tag_value_as<llfix::FixedPoint>(test_tag, decimal_points).get_raw_value(), fixed_point_max, "incoming fix message, value ranges", "FixedPoint max");
    }

    //////////////////////////////////////////////////////////
    // INCOMING REPEATING GROUP
    {
        IncomingFixRepeatingGroups<llfix::FixStringView> rg;
        std::vector<std::string> vals;


        for (std::size_t i = 0; i < 2048; i++)
        {
            vals.push_back(std::to_string(i + 1));
            rg.set(static_cast<uint32_t>(i + 1), get_fix_string_view(vals[i].c_str()));
        }

        unit_test.test_equals(rg.get_value(2048, 0)->to_string() , "2048", "incoming fix repeating group", "set tag beyond initial capacity");
    }

    //////////////////////////////////////////////////////////
    // INCOMING - reset
    {
        IncomingFixMessage msg;

        if (msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        msg.set_tag(333, get_fix_string_view("dirty"));
        msg.reset();
        unit_test.test_equals(msg.has_tag(333), false, "incoming fix message reset", "dirty tag should not be there");

        msg.set_tag(444, get_fix_string_view("new"));
        unit_test.test_equals(msg.has_tag(444), true, "incoming fix message reset", "new tag should be there");
        unit_test.test_equals(msg.get_tag_value_as<std::string>(444), "new", "incoming fix message reset", "new tag value");

        msg.set_tag(333, get_fix_string_view("clean"));
        unit_test.test_equals(msg.has_tag(333), true, "incoming fix message reset", "clean tag should be there");
        unit_test.test_equals(msg.get_tag_value_as<std::string>(333), "clean", "incoming fix message reset", "clean tag value");
    }

    //////////////////////////////////////////////////////////
    // INCOMING - reset repeating group
    {
        IncomingFixMessage msg;

        if (msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        msg.set_tag(35, get_fix_string_view("D"));

        msg.set_repeating_group_tag(448, get_fix_string_view("dirty"));
        msg.reset();

        unit_test.test_equals(msg.has_repeating_group_tag(448), false, "incoming fix message reset rg", "dirty tag should not be there");

        msg.set_repeating_group_tag(448, get_fix_string_view("clean"));
        unit_test.test_equals(msg.has_repeating_group_tag(448), true, "incoming fix message reset rg", "clean tag should be there");
        unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(448, 0), "clean", "incoming fix message reset rg", "clean tag value");

        msg.set_repeating_group_tag(447, get_fix_string_view("new"));
        unit_test.test_equals(msg.has_repeating_group_tag(447), true, "incoming fix message reset rg", "new tag should be there");
        unit_test.test_equals(msg.get_repeating_group_tag_value_as<std::string>(447, 0), "new", "incoming fix message reset rg", "new tag value");
    }

    //////////////////////////////////////////////////////////
    // INCOMING - copy_from
    {
        IncomingFixMessage source_msg;
        IncomingFixMessage target_msg;

        if (source_msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        if (target_msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        source_msg.set_tag(333, get_fix_string_view("dirty"));
        source_msg.reset();
        source_msg.set_tag(444, get_fix_string_view("new"));

        target_msg.copy_non_dirty_tag_values_from(source_msg);

        unit_test.test_equals(target_msg.has_tag(333), false, "incoming fix message copy from", "dirty tag should not be copied");
        unit_test.test_equals(target_msg.has_tag(444), true, "incoming fix message copy from", "new tag should be there");
        unit_test.test_equals(target_msg.get_tag_value_as<std::string>(444), "new", "incoming fix message copy from", "new tag value");

        source_msg.set_tag(333, get_fix_string_view("clean"));
        target_msg.copy_non_dirty_tag_values_from(source_msg);
        unit_test.test_equals(target_msg.has_tag(333), true, "incoming fix message copy from", "clean tag should be there");
        unit_test.test_equals(target_msg.get_tag_value_as<std::string>(333), "clean", "incoming fix message copy from", "clean tag value");
    }

    //////////////////////////////////////////////////////////
    // INCOMING - copy_from repeating group
    {
        IncomingFixMessage source_msg;
        IncomingFixMessage target_msg;

        if (source_msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        if (target_msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        source_msg.set_tag(35, get_fix_string_view("D"));

        target_msg.set_tag(35, get_fix_string_view("D"));

        source_msg.set_repeating_group_tag(448, get_fix_string_view("dirty"));
        source_msg.reset();
        source_msg.set_repeating_group_tag(447, get_fix_string_view("new"));

        target_msg.copy_non_dirty_tag_values_from(source_msg);

        unit_test.test_equals(target_msg.has_repeating_group_tag(448), false, "incoming fix message copy from rg", "dirty tag should not be copied");
        unit_test.test_equals(target_msg.has_repeating_group_tag(447), true, "incoming fix message copy from rg", "new tag should be there");
        unit_test.test_equals(target_msg.get_repeating_group_tag_value_as<std::string>(447, 0), "new", "incoming fix message copy from rg", "new tag value");

        source_msg.set_repeating_group_tag(448, get_fix_string_view("clean"));
        target_msg.copy_non_dirty_tag_values_from(source_msg);
        unit_test.test_equals(target_msg.has_repeating_group_tag(448), true, "incoming fix message copy from rg", "clean tag should be there");
        unit_test.test_equals(target_msg.get_repeating_group_tag_value_as<std::string>(448, 0), "clean", "incoming fix message copy from rg", "clean tag value");
    }

    //////////////////////////////////////////////////////////
    // INCOMING - const range based for loop
    {
        IncomingFixMessage msg;

        if (msg.initialise() == false)
        {
            std::cout << "IncomingFixMessage failed\n";
            return -1;
        }

        msg.set_tag(333, get_fix_string_view("dirty"));
        msg.reset();
        msg.set_tag(35, get_fix_string_view("D"));
        msg.set_tag(444, get_fix_string_view("clean"));

        const IncomingFixMessage& const_msg = msg;
        std::size_t tag_count = 0;
        bool found_msg_type = false;
        bool found_clean_tag = false;
        bool found_dirty_tag = false;

        for (const auto& item : const_msg)
        {
            ++tag_count;

            if (item.key == 35 && item.value.value->to_string() == "D")
            {
                found_msg_type = true;
            }

            if (item.key == 444 && item.value.value->to_string() == "clean")
            {
                found_clean_tag = true;
            }

            if (item.key == 333)
            {
                found_dirty_tag = true;
            }
        }

        unit_test.test_equals(tag_count, static_cast<std::size_t>(2), "incoming fix message const range loop", "should iterate only live tags");
        unit_test.test_equals(found_msg_type, true, "incoming fix message const range loop", "msg type tag should be iterated");
        unit_test.test_equals(found_clean_tag, true, "incoming fix message const range loop", "clean tag should be iterated");
        unit_test.test_equals(found_dirty_tag, false, "incoming fix message const range loop", "dirty tag should not be iterated");
    }

    //////////////////////////////////////////////////////////
    // TIMESTAMP PRECISIONS
    {
        llfix::FixString str;

        llfix::FixUtilities::encode_current_time(&str, llfix::VDSO::SubsecondPrecision::NANOSECONDS);
        unit_test.test_equals(str.length(), 27, "timestamp precisions", "nanosecond precision length");

        llfix::FixUtilities::encode_current_time(&str, llfix::VDSO::SubsecondPrecision::MICROSECONDS);
        unit_test.test_equals(str.length(), 24, "timestamp precisions", "microsecond precision length");

        llfix::FixUtilities::encode_current_time(&str, llfix::VDSO::SubsecondPrecision::MILLISECONDS);
        unit_test.test_equals(str.length(), 21, "timestamp precisions", "millisecond precision length");

        llfix::FixUtilities::encode_current_time(&str, llfix::VDSO::SubsecondPrecision::NONE);
        unit_test.test_equals(str.length(), 17, "timestamp precisions", "no precision length");
    }

    // PRINT THE REPORT
    cout << unit_test.get_summary_report("Fix messages");
    cout.flush();

    #if _WIN32
    bool pause = true;
    if(argc > 1)
    {
        if (std::strcmp(argv[1], "no_pause") == 0)
            pause = false;
    }
    if(pause)
        std::system("pause");
    #else
    LLFIX_UNUSED(argc);
    LLFIX_UNUSED(argv);
    #endif

    return unit_test.did_all_pass();
}

FixStringView* get_fix_string_view(const char* val)
{
    FixStringView* ret = new FixStringView;
    ret->set_buffer(const_cast<char*>(val), strlen(val));
    return ret;
}

std::size_t get_string_count(const std::string& content, std::string needle)
{
    std::size_t count = 0;

    std::string::size_type pos = 0;

    while ((pos = content.find(needle, pos)) != std::string::npos)
    {
        ++count;
        pos += needle.length();
    }

    return count;
}