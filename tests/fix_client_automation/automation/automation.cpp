///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#ifndef LLFIX_ENABLE_DICTIONARY
#define LLFIX_ENABLE_DICTIONARY
#endif

#ifdef __linux__
//#define LLFIX_ENABLE_TCPDIRECT // Enable only in Linux and if there is Solarflare NIC available
#endif

#ifndef LLFIX_AUTOMATION
#define LLFIX_AUTOMATION
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <llfix/engine.h>
#include <llfix/fix_client_settings.h>

#include "order.h"
#include "test_client.h"

#include <llfix/core/compiler/builtin_functions.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/utilities/configuration.h>
#include <llfix/core/utilities/object_cache.h>

#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iostream>

static std::size_t              THROTTLE_LIMIT = 0; // disables it
static constexpr uint64_t       THROTTLE_WINDOW_IN_MILLISECONDS = 1000;
static constexpr std::size_t    HEARTBEAT_INTERVAL_SECONDS = 30;

static std::string    CONFIG_FILE = "config.cfg";
static std::string    FILE_NEW_ORDERS = "new_orders.txt";
static std::string    FILE_REPLACE_ORDERS = "replace_orders.txt";
static std::string    FILE_CANCEL_ORDERS = "cancel_orders.txt";
static std::string    FILE_OUTGOING_RESEND_REQUESTS = "outgoing_resend_requests.txt";

static std::string    OUTGOING_SERIALISATIONS_TEXT_FILE = "outgoing.txt";
static std::string    INCOMING_SERIALISATIONS_TEXT_FILE = "incoming.txt";

std::string get_file_content(const std::string& file_path)
{
    std::ifstream t(file_path);
    std::string content((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    return content;
}

void output(const std::string& message)
{
    std::cout << message << "\n";
}

std::size_t get_string_count(const std::string& file_path, std::string needle)
{
    std::size_t count = 0;

    auto content = get_file_content(file_path);
    std::string::size_type pos = 0;

    while ((pos = content.find(needle, pos)) != std::string::npos)
    {
        ++count;
        pos += needle.length();
    }

    return count;
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        output("Usage: automation <fix_version> <server_in_gap_fill_mode>");
        return -1;
    }

    std::string fix_version = argv[1];
    bool server_in_gap_fill_mode = argv[2][0] == '1' ? true : false;

    llfix::Configuration automation_config;
    std::string automation_config_load_error;

    if (automation_config.load_from_file(CONFIG_FILE, automation_config_load_error) == false)
    {
        output("Could not load automation config");
        return -1;
    }

    llfix::FileSystemUtilities::delete_directory_if_exists("messages_outgoing");
    llfix::FileSystemUtilities::delete_directory_if_exists("messages_incoming");
    llfix::FileSystemUtilities::delete_file_if_exists("sequence.store");
    llfix::FileSystemUtilities::delete_file_if_exists(FILE_NEW_ORDERS);
    llfix::FileSystemUtilities::delete_file_if_exists(FILE_REPLACE_ORDERS);
    llfix::FileSystemUtilities::delete_file_if_exists(FILE_CANCEL_ORDERS);
    llfix::FileSystemUtilities::delete_file_if_exists(INCOMING_SERIALISATIONS_TEXT_FILE);
    llfix::FileSystemUtilities::delete_file_if_exists(OUTGOING_SERIALISATIONS_TEXT_FILE);
    llfix::FileSystemUtilities::delete_file_if_exists(FILE_OUTGOING_RESEND_REQUESTS);
    llfix::FileSystemUtilities::delete_file_if_exists("log.txt");

    auto new_order_count = automation_config.get_int_value("new_order_count", (int)1000);
    auto replace_order_count = automation_config.get_int_value("replace_order_count", (int)1000);

    auto expected_full_fill_count = new_order_count;
    auto expected_new_order_ack_count = replace_order_count;
    auto expected_replace_order_ack_count = replace_order_count;
    auto expected_cancel_ack_count = replace_order_count;
    auto expected_outgoing_new_order_count = new_order_count + replace_order_count;
    auto expected_outgoing_replace_order_count = replace_order_count;
    auto expected_outgoing_cancel_order_count = replace_order_count;

    auto expected_incoming_35_j = 1;
    auto expected_incoming_35_3 = 1;


    std::string deserialiser_executable = "deserialiser.exe";

    #ifdef __linux__
    deserialiser_executable = "./deserialiser";
    #endif

    llfix::Engine::on_start(CONFIG_FILE);

    llfix::ObjectCache<Order> order_cache;

    if (order_cache.create(20480) == false)
    {
        output("Could not create order_cache");
        return -1;
    }

    llfix::FixClientSettings client_settings;

    #ifdef LLFIX_ENABLE_TCPDIRECT
    client_settings.primary_address = automation_config.get_string_value("solarflare_tcpdirect_target_ip");
    client_settings.nic_name = automation_config.get_string_value("solarflare_tcpdirect_nic_name");
    client_settings.nic_address = automation_config.get_string_value("solarflare_tcpdirect_nic_ip");
    #else
    client_settings.primary_address = "127.0.0.1";
    #endif

    llfix::FixSessionSettings session_settings;

    session_settings.set_heartbeat_interval_in_nanoseconds(static_cast<int>(HEARTBEAT_INTERVAL_SECONDS));
    session_settings.throttle_window_in_milliseconds = THROTTLE_WINDOW_IN_MILLISECONDS;
    session_settings.throttle_limit = THROTTLE_LIMIT;

    session_settings.validate_repeating_groups = true;

    if (fix_version == "5")
    {
        client_settings.primary_port = 5024;

        session_settings.begin_string = "FIXT.1.1";

        session_settings.default_app_ver_id = "9";

        session_settings.logon_username = automation_config.get_string_value("automation_username");
        session_settings.logon_password = automation_config.get_string_value("automation_password");
    }
    else if (fix_version == "4.4")
    {
        client_settings.primary_port = 5023;

        session_settings.begin_string = "FIX.4.4";

        session_settings.logon_username = automation_config.get_string_value("automation_username");
        session_settings.logon_password = automation_config.get_string_value("automation_password");
    }
    else if (fix_version == "4.3")
    {
        client_settings.primary_port = 5022;

        session_settings.begin_string = "FIX.4.3";

        session_settings.logon_username = automation_config.get_string_value("automation_username");
        session_settings.logon_password = automation_config.get_string_value("automation_password");
    }
    else if (fix_version == "4.2")
    {
        client_settings.primary_port = 5021;

        session_settings.begin_string = "FIX.4.2";

        #ifdef LLFIX_ENABLE_DICTIONARY
        session_settings.application_dictionary_path = "../../dictionaries/FIX42.xml";
        #endif

    }

    session_settings.sender_comp_id = "CLIENT1";
    session_settings.target_comp_id = "EXECUTOR";
    session_settings.replay_messages_on_incoming_resend_request = true;

    session_settings.additional_static_header_tags = "50=SNDR_SUB,57=SRVR_SUB";

    session_settings.max_serialised_file_size = automation_config.get_int_value("serialisation_file_size");

    session_settings.initialise_derived_settings();

    TestClient client;

    bool creation_success = client.create("EXAMPLE_CLIENT", client_settings, "EXAMPLE_SESSION", session_settings);

    if (creation_success == false)
    {
        output("Could not create client");
        return -1;
    }

    if(fix_version == "4.2" || fix_version == "4.1" || fix_version == "4.0")
    {
        client.set_use_repeating_groups(false);
    }

    bool connection_success = client.connect();

    if (connection_success == false)
    {
        output("Connection to server failed. Please start the venue simulator before runnning this benchmark and check client's login settings.");
        return -1;
    }

    while(true)
    {
        client.process_incoming_messages();

        auto session_state = client.get_session()->get_state();

        if( session_state == llfix::SessionState::LOGGED_ON )
        {
            break;
        }

        if (session_state == llfix::SessionState::LOGON_REJECTED)
        {
            output("Logon rejected. Check username and password");
            return -1;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // NEW ORDERS
    for(std::size_t i =0; i< static_cast<std::size_t>(new_order_count); i++)
    {
        client.process();

        auto new_order = order_cache.allocate();
        new_order->set_symbol(automation_config.get_string_value("fill_instrument"));
        new_order->set_price(10000);
        new_order->set_remaining_qty(1);

        client.send_new_order<>(new_order);
    }

    while (true)
    {
        client.process();
        if (client.execution_report_count() >= static_cast<std::size_t>(new_order_count))
        {
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // VERIFY NEW ORDERS
    auto orders = client.get_orders_as_string();
    llfix::FileSystemUtilities::append_text_to_file(FILE_NEW_ORDERS, orders);

    auto filled_count = get_string_count(FILE_NEW_ORDERS, "FILLED");
    auto content = get_file_content(FILE_NEW_ORDERS);

    if (filled_count == static_cast<std::size_t>(new_order_count))
    {
        output("Execution report verification success : all new orders are filled");
    }
    else
    {
        output("Execution report verification failed : all new orders are not filled");
        return -1;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // REPLACES
    client.reset_execution_report_count();
    std::string symbol = "NOKIA.HE";

    std::vector<Order*> orig_orders_for_replace_orders;
    std::vector<uint32_t> order_ids_for_replace_orders;

    // FIRST SEND NEW ORDERS
    for (std::size_t i = 0; i < static_cast<std::size_t>(replace_order_count); i++)
    {
        client.process();

        auto new_order = order_cache.allocate();
        new_order->set_symbol(symbol);
        new_order->set_price(10000);
        new_order->set_remaining_qty(1);

        client.send_new_order<>(new_order);
        auto current_order_id = client.get_order_id();

        orig_orders_for_replace_orders.push_back(new_order);
        order_ids_for_replace_orders.push_back(current_order_id);
    }

    while (true)
    {
        client.process();
        if (client.execution_report_count() >= static_cast<std::size_t>(replace_order_count))
        {
            break;
        }
    }

    client.reset_execution_report_count();

    // NOW SEND REPLACE ORDERS
    std::vector<Order*> replace_orders;
    std::size_t counter = 0;
    for(auto& orig_order : orig_orders_for_replace_orders)
    {
        client.process();

        auto orig_order_id = order_ids_for_replace_orders[counter];

        auto replace_order = order_cache.allocate();

        replace_order->set_symbol(orig_order->get_symbol());
        replace_order->set_numeric_order_id(orig_order_id);
        replace_order->set_remaining_qty(2);
        replace_order->set_price(10000);

        replace_order->set_type(orig_order->get_type());
        replace_order->set_side(orig_order->get_side());

        client.send_replace_order(replace_order, orig_order);
        replace_orders.push_back(replace_order);

        counter++;
    }

    while (true)
    {
        client.process();

        if (client.execution_report_count() >= static_cast<std::size_t>(replace_order_count))
        {
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // VERIFY REPLACE ORDERS
    orders = client.get_orders_as_string();
    llfix::FileSystemUtilities::append_text_to_file(FILE_REPLACE_ORDERS, orders);

    auto replaced_count = get_string_count(FILE_REPLACE_ORDERS, "Remaining qty : 2");
    content = get_file_content(FILE_REPLACE_ORDERS);

    if (replaced_count == static_cast<std::size_t>(replace_order_count))
    {
        output("Execution report verification success : all replace orders were acked");
    }
    else
    {
        output("Execution report verification failed : all replace orders were not acked");
        return -1;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // SENDING WRONG MESSAGES TO TRIGGER REJECTS FROM THE SERVER

    // 35=D message below has so many missing fields. This will trigger a 35=j message from the 5.0 server and 35=3 from 4.4 4.3 4.2 servers
    auto incomplete_message = client.outgoing_message_instance();
    incomplete_message->set_msg_type('D');
    incomplete_message->set_tag(1, "abcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefghabcdefgh");
    client.send_outgoing_message(incomplete_message);

    if (fix_version != "5")
    {
        expected_incoming_35_j--;
        expected_incoming_35_3++;
    }

    expected_outgoing_new_order_count += 1;

    // 35=0 message below has a tag that server won't recognise. This will trigger a 35=3 message from the server
    auto message_with_unknown_tag = client.outgoing_message_instance();
    message_with_unknown_tag->set_msg_type('0');
    message_with_unknown_tag->set_tag(99991, "asdas");
    client.send_outgoing_message(message_with_unknown_tag);

    // 35=D reject
    client.reset_execution_report_count();

    auto new_order = order_cache.allocate();
    new_order->set_symbol(automation_config.get_string_value("reject_instrument"));
    new_order->set_price(10000);
    new_order->set_remaining_qty(1);

    client.send_new_order<>(new_order);
    expected_outgoing_new_order_count += 1;

    while (true)
    {
        client.process();

        if (client.execution_report_count() >= 1)
        {
            break;
        }
    }

    // 35=F reject
    client.send_cancel_order(new_order);
    expected_outgoing_cancel_order_count++;

    while (true)
    {
        client.process();

        if (client.order_cancel_replace_reject_count() == 1)
        {
            break;
        }
    }

    // 35=G reject
    auto replace_order = order_cache.allocate();

    replace_order->set_symbol(new_order->get_symbol());
    replace_order->set_numeric_order_id(44345);
    replace_order->set_remaining_qty(2);
    replace_order->set_price(10000);

    replace_order->set_type(new_order->get_type());
    replace_order->set_side(new_order->get_side());

    client.send_replace_order(replace_order, new_order);
    expected_outgoing_replace_order_count++;

    while (true)
    {
        client.process();

        if (client.order_cancel_replace_reject_count() == 2)
        {
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // CANCELS
    client.reset_execution_report_count();

    for(auto& orig_order : replace_orders)
    {
        client.process();

        client.send_cancel_order(orig_order);
    }

    while (true)
    {
        client.process();

        if (client.execution_report_count() >= static_cast<std::size_t>(replace_order_count))
        {
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // VERIFY CANCEL ORDERS
    orders = client.get_orders_as_string();
    llfix::FileSystemUtilities::append_text_to_file(FILE_CANCEL_ORDERS, orders);

    auto cancelled_count = get_string_count(FILE_CANCEL_ORDERS, "CANCELLED");
    content = get_file_content(FILE_CANCEL_ORDERS);

    if (cancelled_count == static_cast<std::size_t>(replace_order_count))
    {
        output("Execution report verification success : all cancel orders were cancelled");
    }
    else
    {
        output("Execution report verification failed : all cancel orders were not cancelled");
        return -1;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // SENDING RESEND REQUESTS
    client.reset_execution_report_count();
    auto orig_incoming_seq_no = client.get_session()->get_sequence_store()->get_incoming_seq_no();

    // DECREMENTING INCOMING SEQ NO TO TRIGGER SENDING A RESEND REQUEST
    client.get_session()->get_sequence_store()->set_incoming_seq_no(orig_incoming_seq_no - 5);

    // Sending a new order to trigger a msg from server
    new_order = order_cache.allocate();
    new_order->set_symbol(symbol);
    new_order->set_price(10000);
    new_order->set_remaining_qty(1);

    client.send_new_order<>(new_order);

    if (server_in_gap_fill_mode == false)
    {
        while (true)
        {
            client.process();

            if (client.execution_report_count() == 6)
            {
                break;
            }
        }
    }
    else
    {
        auto start = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(10000);

        while(true)
        {
            client.process();

            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout)
            {
                break;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////
    // VERIFY RESULT OF SENT RESEND REQUESTS : if resend request succeeds we shall get a new order ack and there should be no pending new orders
    if (server_in_gap_fill_mode == false)
    {
        orders = client.get_orders_as_string();
        llfix::FileSystemUtilities::append_text_to_file(FILE_OUTGOING_RESEND_REQUESTS, orders);

        auto pending_new_order_count = get_string_count(FILE_OUTGOING_RESEND_REQUESTS, "PENDING_NEW_ORDER");

        if (pending_new_order_count == 0)
        {
            output("Outgoing resend request was successful");
        }
        else
        {
            output("Outgoing resend request was not successful");
            return -1;
        }

        expected_outgoing_new_order_count += 1; // +1 because of triggering outgoing resend request
        expected_cancel_ack_count += 5; // +5 is due to the sent resend request. So we get last 5 cancel acks twice
        expected_new_order_ack_count += 2; // +2 is from the new order sent to trigger resend request. Since we send a resend request we get its ack two times
    }
    else
    {
        expected_outgoing_new_order_count += 1; // +1 because of triggering outgoing resend request
        expected_new_order_ack_count += 1; // +1 is from the new order sent to trigger resend request, however in this mode server will not resend the ack, but gap fill instead
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // TRIGGERRING RESEND REQUEST FROM SERVER WHEN WE ARE IN GAP-FILL MODE
    client.get_session()->set_replay_messages_on_incoming_resend_request(false);

    // Send fake orders
    for(std::size_t i=0;i<5;i++)
    {
        auto new_order = order_cache.allocate();
        new_order->set_symbol("FAKE");
        new_order->set_price(10000);
        new_order->set_remaining_qty(1);

        client.send_new_order<>(new_order, true);
    }

    // Now send a real order so that server will detect the gap and send a resend request
    new_order = order_cache.allocate();
    new_order->set_symbol(symbol);
    new_order->set_price(10000);
    new_order->set_remaining_qty(1);

    client.send_new_order<>(new_order);

    expected_outgoing_new_order_count += 6;
    expected_new_order_ack_count += 5; // The actual order we send to trigger other side has 34=4015, and we send a gap fill with 36=4016
                                       // hence by gap filling we allow the last sent order to be ignored which is the expected case of this test

    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(10000);

    while (true)
    {
        client.process();

        auto now = std::chrono::steady_clock::now();
        if (now - start >= timeout)
        {
            break;
        }
    }

    client.send_heartbeat(false); // sending a regular message after gap fill so that quickfix gets into "satisfied" state

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // TRIGGERRING RESEND REQUEST FROM SERVER WHEN WE ARE IN MESSAGE REPLAY MODE
    client.get_session()->set_replay_messages_on_incoming_resend_request(true);

    // Sending fake orders
    for (std::size_t i = 0; i < 5; i++)
    {
        auto new_order = order_cache.allocate();
        new_order->set_symbol("FAKE");
        new_order->set_price(10000);
        new_order->set_remaining_qty(1);

        client.send_new_order<>(new_order, true);
    }

    // And fake heartbeat
    client.send_heartbeat(true);

    // Now send a real order so that server will detect the gap and send a resend request
    new_order = order_cache.allocate();
    new_order->set_symbol(symbol);
    new_order->set_price(10000);
    new_order->set_remaining_qty(1);

    client.send_new_order<>(new_order);

    expected_outgoing_new_order_count += 12; // We have sent additional 6 new orders and msg resend will also serialise outgoing resent messages with 43=Y so another 6
    expected_new_order_ack_count += 1;

    start = std::chrono::steady_clock::now();
    timeout = std::chrono::milliseconds(10000);

    while (true)
    {
        client.process();

        auto now = std::chrono::steady_clock::now();
        if (now - start >= timeout)
        {
            break;
        }
    }

    // send one more order to check all is good after resend
    client.reset_execution_report_count();

    new_order = order_cache.allocate();
    new_order->set_symbol(symbol);
    new_order->set_price(10000);
    new_order->set_remaining_qty(1);

    client.send_new_order<>(new_order);

    while (true)
    {
        client.process();

        if (client.execution_report_count() >= 1)
        {
            break;
        }
    }

    expected_outgoing_new_order_count += 1;
    expected_new_order_ack_count += 1;

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // TESTING IF CAN READ FROM OLDER SERIALISED FILES
    char test_buffer[2048];
    std::size_t read_length = 0;

    if (client.get_session()->get_outgoing_message_serialiser()->read_message(10, test_buffer, 2048, read_length) == false)
    {
        output("ERROR : Failed to read earlier messages from serialisations !!!");
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // TESTING RESPONSES TO INCOMING TEST REQUESTS
    while (true)
    {
        client.process_incoming_messages(); // not calling process so no outgoing, eventually the peer will send a test request

        if (client.get_session()->needs_responding_to_incoming_test_request())
        {
            client.process();
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // IF INCOMING SEQ NO IS TOO LOW , WE SHOULD DISCONNECT
    orig_incoming_seq_no = client.get_session()->get_sequence_store()->get_incoming_seq_no();
    client.get_session()->get_sequence_store()->set_incoming_seq_no(orig_incoming_seq_no + 5);

    new_order = order_cache.allocate();
    new_order->set_symbol(symbol);
    new_order->set_price(10000);
    new_order->set_remaining_qty(1);
    client.send_new_order<>(new_order);

    expected_outgoing_new_order_count += 1;
    expected_new_order_ack_count += 1;

    start = std::chrono::steady_clock::now();
    timeout = std::chrono::milliseconds(10000);

    while (true)
    {
        client.process();

        auto now = std::chrono::steady_clock::now();
        if (now - start >= timeout)
        {
            break;
        }

        if (client.get_session()->get_state() == llfix::SessionState::DISCONNECTED)
        {
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // VERIFY OUTGOING SERIALISATIONS
    auto validate = [&](std::size_t actual, std::size_t expected, std::string message)
        {
            if (actual == expected)
            {
                output(message + " => SUCCESS");
                return true;
            }
            else
            {
                output(message + " => FAIL");
                return false;
            }
        };

    auto deserialiser_outgoing_command = deserialiser_executable + " -i messages_outgoing -o " + OUTGOING_SERIALISATIONS_TEXT_FILE;

    output("Running command : " + deserialiser_outgoing_command);

    auto res = std::system(deserialiser_outgoing_command.c_str());

    if(res != 0 )
    {
        output("Failed to deserialise outgoing messages");
        return -1;
    }

    validate((std::size_t)client.get_session()->get_state(), (std::size_t)llfix::SessionState::DISCONNECTED, "FixClient should disconnect in case of a low incoming seq no");

    auto outgoing_replay_message = get_string_count(OUTGOING_SERIALISATIONS_TEXT_FILE, "43=Y");
    validate(outgoing_replay_message, 7, "Number of outgoing messages with 43=Y (resent messages)");

    auto outgoing_gap_fill_count = get_string_count(OUTGOING_SERIALISATIONS_TEXT_FILE, "35=4");
    validate(outgoing_gap_fill_count, 2, "Number of outgoing 35=4 messages"); // Gap fill + heartbeat will be replaced with 35=4 during in resend mode so 2

    auto outgoing_test_request_response_count = get_string_count(OUTGOING_SERIALISATIONS_TEXT_FILE, "112=TEST");
    validate(outgoing_test_request_response_count, 1, "Number of outgoing test request responses");

    auto outgoing_resend_request_count = get_string_count(OUTGOING_SERIALISATIONS_TEXT_FILE, "35=2");
    validate(outgoing_resend_request_count, 1, "Number of outgoing 35=2 messages");

    auto outgoing_35_d_count = get_string_count(OUTGOING_SERIALISATIONS_TEXT_FILE, "35=D");
    validate(outgoing_35_d_count, expected_outgoing_new_order_count, "Number of outgoing 35=D messages");

    auto outgoing_35_g_count = get_string_count(OUTGOING_SERIALISATIONS_TEXT_FILE, "35=G");
    validate(outgoing_35_g_count, expected_outgoing_replace_order_count, "Number of outgoing 35=G messages");

    auto outgoing_35_f_count = get_string_count(OUTGOING_SERIALISATIONS_TEXT_FILE, "35=F");
    validate(outgoing_35_f_count, expected_outgoing_cancel_order_count, "Number of outgoing 35=F messages");

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // VERIFY INCOMING SERIALISATIONS
    auto deserialiser_incoming_command = deserialiser_executable + " -i messages_incoming -o " + INCOMING_SERIALISATIONS_TEXT_FILE;
    output("Running command : " + deserialiser_incoming_command);

    res = std::system(deserialiser_incoming_command.c_str());

    if(res != 0 )
    {
        output("Failed to deserialise incoming messages");
        return -1;
    }

    auto incoming_test_request_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "35=1");
    validate(incoming_test_request_count, 1, "Number of incoming 35=1 messages");

    auto incoming_j_reject_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "35=j");
    validate(incoming_j_reject_count, expected_incoming_35_j, "Number of incoming 35=j messages");

    auto incoming_3_reject_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "35=3");
    validate(incoming_3_reject_count, expected_incoming_35_3, "Number of incoming 35=3 messages");

    auto incoming_cancel_replace_reject_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "35=9");
    validate(incoming_cancel_replace_reject_count, 2, "Number of incoming 35=9 messages");

    auto incoming_resend_request_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "35=2");
    validate(incoming_resend_request_count, 2, "Number of incoming 35=2 messages");

    auto incoming_new_order_reject_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "150=8");
    validate(incoming_new_order_reject_count, 1, "Number of incoming 150=8 messages");

    auto incoming_full_fill_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "150=2");
    validate(incoming_full_fill_count, expected_full_fill_count, "Number of incoming 150=2 messages");

    auto incoming_new_order_ack_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "150=0");
    validate(incoming_new_order_ack_count, expected_new_order_ack_count, "Number of incoming 150=0 messages");

    auto incoming_replace_ack_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "150=5");
    validate(incoming_replace_ack_count, expected_replace_order_ack_count, "Number of incoming 150=5 messages");

    auto incoming_cancel_ack_count = get_string_count(INCOMING_SERIALISATIONS_TEXT_FILE, "150=4");
    validate(incoming_cancel_ack_count, expected_cancel_ack_count, "Number of incoming 150=4 messages");

    //////////////////////////////////////////////////////////////////////////////////////////////////

    #if _WIN32
    std::system("pause");
    #endif

    return 0;
}