///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#ifndef LLFIX_UNIT_TEST
#define LLFIX_UNIT_TEST
#endif

#ifndef LLFIX_ENABLE_BINARY_FIELDS
#define LLFIX_ENABLE_BINARY_FIELDS
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <llfix/common.h>
#include "../unit_test.h"

#include <cstdlib>
#include <cstring>

#include <iostream>
using namespace std;

#include <string>

#include <llfix/engine.h>
#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/tcp_connector.h>
#include <llfix/fix_client.h>
#include <llfix/fix_client_settings.h>
#include <llfix/fix_utilities.h>

using namespace llfix;

#include <llfix/core/utilities/filesystem_utilities.h>

UnitTest unit_test;

class DummyClient : public llfix::FixClient<llfix::TCPConnector>
{
    private:

    public:

        int exec_report_count = 0;
        int custom_message_count = 0;

        DummyClient()
        {
            specify_repeating_group("8", 453, 448, 447, 452, 801, 785, 786);
            specify_repeating_group("8", 558, 60);
        }

        void on_execution_report(const llfix::IncomingFixMessage* message) override
        {
            exec_report_count++;
            std::cout << "Exec report : " << message->to_string() << "\n";

            #ifdef LLFIX_ENABLE_BINARY_FIELDS
            if (message->has_tag(96))
            {
                auto binary_val = message->get_tag_value_as<std::string_view>(96);
                unit_test.test_equals(is_expected_binary_val(binary_val.data()), true, "fix parser", "BINARY/RAW DATA VALUE CHECK");
            }
            #endif
        }

        void on_custom_message_type(const llfix::IncomingFixMessage* message) override
        {
            custom_message_count++;
            std::cout << "Custom message : " << message->to_string() << "\n";
        }

        bool is_expected_binary_val(const char* val)
        {
            if (val[0] != 'A') return false;
            if (val[1] != ((char)(1))) return false;
            if (val[2] != '1') return false;
            if (val[3] != '0') return false;

            return true;
        }
};

int main(int argc, char* argv[])
{
    FileSystemUtilities::delete_file_if_exists("sequence.store");
    FileSystemUtilities::delete_directory_if_exists("messages_incoming");
    FileSystemUtilities::delete_directory_if_exists("messages_outgoing");
    FileSystemUtilities::delete_directory_if_exists("client1");

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT SETUP

    llfix::FixSessionSettings session_settings;

    session_settings.begin_string = "FIX.4.2";
    session_settings.sender_comp_id = "TRADER";
    session_settings.target_comp_id = "EXECUTOR";

    session_settings.initialise_derived_settings();
    session_settings.throttle_limit = 5000;

    session_settings.validate_repeating_groups = true;

    llfix::FixClientSettings client_settings;

    client_settings.primary_port = 5001;
    client_settings.primary_address = "127.0.0.1";

    DummyClient* client = new DummyClient;


    bool creation_success = client->create("EXAMPLE_CLIENT", client_settings, "EXAMPLE_SESSION", session_settings);

    if (creation_success == false)
    {
        std::cout << "Could not create client" << std::endl;
        return -1;
    }

    TCPConnectorOptions params;
    params.m_rx_buffer_capacity = 256;
    client->set_params(params);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 1 - COMPLETE SINGLE MESSAGE , NO EXCESS BYTES
    {
        std::string message_buffer = "8=FIX.4.2|9=109|35=8|34=1|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=93|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "COMPLETE SINGLE MESSAGE , NO EXCESS BYTES");
        unit_test.test_equals(client->exec_report_count, 1, "fix parser", "COMPLETE SINGLE MESSAGE , NO EXCESS BYTES");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 2 - COMPLETE DOUBLE MESSAGE , NO EXCESS BYTES
    {
        std::string message_buffer = "8=FIX.4.2|9=109|35=8|34=2|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=94|8=FIX.4.2|9=109|35=8|34=3|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=95|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());
        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "COMPLETE DOUBLE MESSAGE , NO EXCESS BYTES");
        unit_test.test_equals(client->exec_report_count, 2, "fix parser", "COMPLETE SINGLE MESSAGE , NO EXCESS BYTES");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 3 - COMPLETE SINGLE MESSAGE , WITH EXCESS BYTES
    {
        std::string message_buffer = "8=FIX.4.2|9=109|35=8|34=4|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=96|abc";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 3, "fix parser", "COMPLETE SINGLE MESSAGE , WITH EXCESS BYTES");
        unit_test.test_equals(client->exec_report_count, 1, "fix parser", "COMPLETE SINGLE MESSAGE , WITH EXCESS BYTES");
    }
    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 4 - COMPLETE SINGLE MESSAGE , WITH EXCESS BYTES 2
    {
        std::string message_buffer = "8=FIXT.1.1|9=181|35=8|34=172366|49=EXECUTOR|52=20250913-12:56:52.239724763|56=CLIENT59|17=10351359|11=172361|37=172361|150=F|39=2|60=20250913-12:56:52.239724763|32=1|151=0|14=1|31=5|54=1|55=BMWG.DE|10=046|8=FIXT.1.1|9=181|35=8|34=172367|49=EXECUTOR|52=20250913-12:56:52.239727199|56=CLIENT59|17=10351360|11=172362|37=172362|150=F|39=2|60=20250913-12:56:52.239727199|32=1|151=0|14=1|31=5|54=1|55=BMWG.DE|10=0";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->get_session()->set_validations_enabled(false);

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 202, "fix parser", "COMPLETE SINGLE MESSAGE , WITH EXCESS BYTES 2");

        client->get_session()->set_validations_enabled(true);
    }
    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 5 - COMPLETE DOUBLE MESSAGE , WITH EXCESS BYTES
    {
        std::string message_buffer = "8=FIX.4.2|9=109|35=8|34=5|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=97|8=FIX.4.2|9=109|35=8|34=6|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=98|abcff";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 5, "fix parser", "COMPLETE DOUBLE MESSAGE , WITH EXCESS BYTES");
        unit_test.test_equals(client->exec_report_count, 2, "fix parser", "COMPLETE DOUBLE MESSAGE , WITH EXCESS BYTES");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 6 - INCOMPLETE BUFFER
    {
        std::string message_buffer = "8=FIX.4.2|9=109|35=8|34=7|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=93|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length() -1 ); // WE MAKE IT INCOMPLETE WITH -1

        unit_test.test_equals(client->get_incomplete_buffer_size(), message_buffer.length() - 1, "fix parser", "INCOMPLETE BUFFER");
        unit_test.test_equals(client->exec_report_count, 0, "fix parser", "INCOMPLETE BUFFER");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 7 - REPEATING GROUP
    {
        std::string message_buffer = "8=FIX.4.2|9=192|35=8|34=7|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|453=2|448=PARTY1|447=D|452=1|448=PARTY2|447=D|452=3|60=20250913-12:56:52.239724763|10=034|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "REPEATING GROUP INCOMPLETE BUFFER");
        unit_test.test_equals(client->exec_report_count, 1, "fix parser", "REPEATING GROUP EXEC REPORT COUNT");

        auto incoming_message = client->get_session()->get_incoming_fix_message();

        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<uint32_t>(453, 0), 2, "fix parser", "REPEATING GROUP TAG 453");

        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<uint32_t>(452, 0), 1, "fix parser", "REPEATING GROUP TAG 452 1");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<std::string>(448, 0), "PARTY1", "fix parser", "REPEATING GROUP TAG 448 1");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<char>(447, 0), 'D', "fix parser", "REPEATING GROUP TAG 447 1");

        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<uint32_t>(452, 1), 3, "fix parser", "REPEATING GROUP TAG 452 2");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<std::string>(448, 1), "PARTY2", "fix parser", "REPEATING GROUP TAG 448 2");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<char>(447, 1), 'D', "fix parser", "REPEATING GROUP TAG 447 2");

        unit_test.test_equals(incoming_message->has_tag(60), true, "fix parser", "REPEATING GROUP TAG END DETECTION has tag");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 8 - NESTED REPEATING GROUP
    {
        std::string message_buffer = "8=FIX.4.2|9=158|35=8|34=8|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|453=1|448=PARTY1|447=D|452=1|801=1|785=STR|786=1|10=053|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "NESTED REPEATING GROUP INCOMPLETE BUFFER");
        unit_test.test_equals(client->exec_report_count, 1, "fix parser", "NESTED REPEATING GROUP EXEC REPORT COUNT");

        auto incoming_message = client->get_session()->get_incoming_fix_message();

        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<uint32_t>(453, 0), 1, "fix parser", "NESTED REPEATING GROUP TAG 453");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<uint32_t>(452, 0), 1, "fix parser", "NESTED REPEATING GROUP TAG 452");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<std::string>(448, 0), "PARTY1", "fix parser", "NESTED REPEATING GROUP TAG 448");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<char>(447, 0), 'D', "fix parser", "NESTED REPEATING GROUP TAG 447");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<uint32_t>(801, 0), 1, "fix parser", "NESTED REPEATING GROUP TAG 801");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<std::string>(785, 0), "STR", "fix parser", "NESTED REPEATING GROUP TAG 785");
        unit_test.test_equals(incoming_message->get_repeating_group_tag_value_as<uint32_t>(786, 0), 1, "fix parser", "NESTED REPEATING GROUP TAG 786");
    }

    client->exec_report_count = 0;
    client->custom_message_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 9 - MSGTYPES WITH MULTICHARS
    {
        std::string message_buffer = "8=FIX.4.2|9=110|35=AS|34=9|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=186|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "MSGTYPES WITH MULTICHARS INCOMPLETE BUFFER");
        unit_test.test_equals(client->custom_message_count, 1, "fix parser", "MSGTYPES WITH MULTICHARS CUSTOM MESSAGE COUNT");
    }

    client->exec_report_count = 0;
    client->custom_message_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 10 - GARBAGE BUFFER ( BUFFERS THAT DON'T START WITH BEGIN STRING )
    {
        std::string message_buffer = "aaa|9=110|35=8|34=10|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=93|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "GARBAGE BUFFER");
        unit_test.test_equals(client->exec_report_count, 0, "fix parser", "GARBAGE BUFFER");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 11 - GARBAGE BUFFER ( BUFFERS THAT DON'T START WITH BEGIN STRING ) + VALID MESSAGE
    {
        std::string message_buffer = "aaabbbbb|8=FIX.4.2|9=110|35=8|34=10|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=134|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "MESSAGE AFTER GARBAGE BUFFER");
        unit_test.test_equals(client->exec_report_count, 1, "fix parser", "MESSAGE AFTER GARBAGE BUFFER");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 12 - GARBAGE BUFFER ( BUFFERS THAT DON'T START WITH BEGIN STRING ) + VALID MESSAGE x 2
    {
        std::string message_buffer = "aaabbbbb|8=FIX.4.2|9=110|35=8|34=11|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=135|fgdggdgdfgdgdfg8=FIX.4.2|9=110|35=8|34=12|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|10=136|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "MESSAGE AFTER GARBAGE BUFFER x2");
        unit_test.test_equals(client->exec_report_count, 2, "fix parser", "MESSAGE AFTER GARBAGE BUFFER x2");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 13 - RAW/BINARY DATA THAT CONTAINS SOH
    #ifdef LLFIX_ENABLE_BINARY_FIELDS
    {
        
        std::string message_buffer = "8=FIX.4.2|9=123|35=8|34=13|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|95=4|96=Az10|10=189|";
        for (char& ch : message_buffer)
        {
            if (ch == '|' || ch == 'z') // z will apply to t96 value we will have SOH inside tag 96's value
            {
                ch = (char)(1);
            }
        }

        client->specify_binary_field("8", 95, 96);
        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 0, "fix parser", "RAW/BINARY DATA WITH EQUALS SIGN");
        unit_test.test_equals(client->exec_report_count, 1, "fix parser", "RAW/BINARY DATA WITH EQUALS SIGN");
    }

    client->exec_report_count = 0;
    #endif

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CLIENT 14 - MALFORMED MESSAGE
    {
        std::string message_buffer = "8=FIXT.1.1|9=242|8=FIXT.1.1|9=242|35=D|49=CLIENT1|56=EXECUTOR|34=2|43=Y|52=20260302-16:21:53.279|122=20260302-16:21:53.279|11=12|453=2|448=PARTY1|447=D|452=1|448=PARTY2|447=D|452=3|55=NOKIA.HE|54=1|60=20260302-16:21:53.279|38=10.0|40=2|44=10000.0|59=0|10=191|10=186|8=FIXT.1.1|9=242|8=FIXT.1.1|9=242|35=D|49=CLIENT1|56=EXECUTOR|34=2|43=Y|52=20260302-16:21:53.279|122=20260302-16:21:53.279|11=13|453=2|448=PARTY1|447=D|452=1|448=PARTY2|447=D|452=3|55=NOKIA.HE|54=1|60=20260302-16:21:53.279|38=10.0|40=2|44=10000.0|59=0|10=186|10=191|8=FIXT.1.1|9=242|8=FIXT.1.1|9=242|35=D|49=CLIENT1|56=EXECUTOR|34=2|43=Y|52=20260302-16:21:53.279|122=20260302-16:21:53.279|11=14|453=2|448=PARTY1|447=D|452=1|448=PARTY2|447=D|452=3|55=NOKIA.HE|54=1|60=20260302-16:21:53.279|38=10.0|40=2|44=10000.0|59=0|10=191|10=188|8=FIXT.1.1|9=242|8=FIXT.1.1|9=242|35=D|49=CLIENT1|56=EXECUTOR|34=2|43=Y|52=20260302-16:21:53.279|122=20260302-16:21:53.279|11=15|453=2|448=PARTY1|447=D|452=1|448=PARTY2|447=D|452=3|55=NOKIA.HE|54=1|60=20260302-16:21:53.279|38=10.0|40=2|44=10000.0|59=0|10=188|10=195|8=FIXT.1.1|9=242|8=FIXT.1.1|9=242|35=D|49=CLIENT1|56=EXECUTOR|34=2|43=Y|52=20260302-16:21:53.279|122=20260302-16:21:53.279|11=16|453=2|448=PARTY1|447=D|452=1|448=PARTY2|447=D|452=3|55=NOKIA.HE|54=1|60=20260302-16:21:53.279|38=10.0|40=2|44=10000.0|59=0|10=195|10=194|8=FIXT.1.1|9=242|8=FIXT.1.1|9=242|35=D|49=CLIENT1|56=EXECUTOR|34=2|43=Y|52=20260302-16:21:53.279|122=20260302-16:21:53";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        client->get_session()->get_sequence_store()->reset_numbers();

        client->reset_incomplete_buffer();
        client->process_rx_buffer(const_cast<char*>(message_buffer.c_str()), message_buffer.length());

        unit_test.test_equals(client->get_incomplete_buffer_size(), 118, "fix parser", "malformed message incomplete buffer size");
        unit_test.test_equals(client->exec_report_count, 0, "fix parser", "malformed message exec report cnt");
    }

    client->exec_report_count = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CHECKSUM UTILITIES
    {
        std::string message_buffer = "8=FIX.4.2|9=109|35=8|34=1|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|";
        for (char& ch : message_buffer)
        {
            if (ch == '|')
            {
                ch = (char)(1);
            }
        }

        char checksum_buffer[3];
         
        unit_test.test_equals(FixUtilities::validate_checksum_no_simd(message_buffer.c_str(), message_buffer.size(), 94), true, "fix utilities validate_checksum_no_simd", "short buffer");
        unit_test.test_equals(FixUtilities::validate_checksum_simd_avx2(message_buffer.c_str(), message_buffer.size(), 94), true, "fix utilities validate_checksum_simd_avx2", "short buffer");

        FixUtilities::encode_checksum_no_simd(message_buffer.c_str(), message_buffer.size(), checksum_buffer);
        unit_test.test_equals(strncmp(checksum_buffer, "094", 3), 0, "fix utilities encode_checksum_no_simd", "short buffer");

        FixUtilities::encode_checksum_simd_avx2(message_buffer.c_str(), message_buffer.size(), checksum_buffer);
        unit_test.test_equals(strncmp(checksum_buffer, "094", 3), 0, "fix utilities encode_checksum_simd_avx2", "short buffer");
    }

    {
        std::string message_buffer(4096, static_cast<char>(0xFF));
        char checksum_no_simd[3];
        char checksum_simd[3];

        FixUtilities::encode_checksum_no_simd(message_buffer.c_str(), message_buffer.size(), checksum_no_simd);
        FixUtilities::encode_checksum_simd_avx2(message_buffer.c_str(), message_buffer.size(), checksum_simd);

        unit_test.test_equals(strncmp(checksum_no_simd, checksum_simd, 3), 0, "fix utilities encode_checksum_simd_avx2", "uint16_t lane overflow boundary 4096");

        uint32_t expected_checksum = ((checksum_no_simd[0] - '0') * 100) + ((checksum_no_simd[1] - '0') * 10) + (checksum_no_simd[2] - '0');

        unit_test.test_equals(FixUtilities::validate_checksum_no_simd(message_buffer.c_str(), message_buffer.size(), expected_checksum), true, "fix utilities validate_checksum_no_simd", "uint16_t lane overflow boundary 4096");
        unit_test.test_equals(FixUtilities::validate_checksum_simd_avx2(message_buffer.c_str(), message_buffer.size(), expected_checksum), true, "fix utilities validate_checksum_simd_avx2", "uint16_t lane overflow boundary 4096");
    }

    {
        std::string message_buffer(4128, static_cast<char>(0xFF));
        char checksum_no_simd[3];
        char checksum_simd[3];

        FixUtilities::encode_checksum_no_simd(message_buffer.c_str(), message_buffer.size(), checksum_no_simd);
        FixUtilities::encode_checksum_simd_avx2(message_buffer.c_str(), message_buffer.size(), checksum_simd);

        unit_test.test_equals(strncmp(checksum_no_simd, checksum_simd, 3), 0, "fix utilities encode_checksum_simd_avx2", "uint16_t lane overflow above 4KB");

        uint32_t expected_checksum = ((checksum_no_simd[0] - '0') * 100) + ((checksum_no_simd[1] - '0') * 10) + (checksum_no_simd[2] - '0');

        unit_test.test_equals(FixUtilities::validate_checksum_no_simd(message_buffer.c_str(), message_buffer.size(), expected_checksum), true, "fix utilities validate_checksum_no_simd", "uint16_t lane overflow above 4KB");
        unit_test.test_equals(FixUtilities::validate_checksum_simd_avx2(message_buffer.c_str(), message_buffer.size(), expected_checksum), true, "fix utilities validate_checksum_simd_avx2", "uint16_t lane overflow above 4KB");
    }

    {
        std::string message_buffer(8192, static_cast<char>(0xFF));
        char checksum_no_simd[3];
        char checksum_simd[3];

        FixUtilities::encode_checksum_no_simd(message_buffer.c_str(), message_buffer.size(), checksum_no_simd);
        FixUtilities::encode_checksum_simd_avx2(message_buffer.c_str(), message_buffer.size(), checksum_simd);

        unit_test.test_equals(strncmp(checksum_no_simd, checksum_simd, 3), 0, "fix utilities encode_checksum_simd_avx2", "uint16_t lane overflow large buffer");

        uint32_t expected_checksum = ((checksum_no_simd[0] - '0') * 100) + ((checksum_no_simd[1] - '0') * 10) + (checksum_no_simd[2] - '0');

        unit_test.test_equals(FixUtilities::validate_checksum_no_simd(message_buffer.c_str(), message_buffer.size(), expected_checksum), true, "fix utilities validate_checksum_no_simd", "uint16_t lane overflow large buffer");
        unit_test.test_equals(FixUtilities::validate_checksum_simd_avx2(message_buffer.c_str(), message_buffer.size(), expected_checksum), true, "fix utilities validate_checksum_simd_avx2", "uint16_t lane overflow large buffer");
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PACK/UNPACK MESSAGE TYPE
    {
        const auto packed_d = FixUtilities::pack_message_type(std::string_view("D", 1));
        unit_test.test_equals(FixUtilities::unpack_message_type(packed_d), std::string("D"), "fix parser msgtype packing", "PACK/UNPACK 1 CHAR");

        const auto packed_as = FixUtilities::pack_message_type(std::string_view("AS", 2));
        unit_test.test_equals(FixUtilities::unpack_message_type(packed_as), std::string("AS"), "fix parser msgtype packing", "PACK/UNPACK 2 CHARS");

        const auto packed_abc = FixUtilities::pack_message_type(std::string_view("ABC", 3));
        unit_test.test_equals(FixUtilities::unpack_message_type(packed_abc), std::string("ABC"), "fix parser msgtype packing", "PACK/UNPACK 3 CHARS");

        const auto packed_abcd = FixUtilities::pack_message_type(std::string_view("ABCD", 4));
        unit_test.test_equals(FixUtilities::unpack_message_type(packed_abcd), std::string("ABCD"), "fix parser msgtype packing", "PACK/UNPACK 4 CHARS");
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PRINT THE REPORT
    cout << unit_test.get_summary_report("Fix parsers");
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
