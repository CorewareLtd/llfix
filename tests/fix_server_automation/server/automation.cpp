///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#ifndef LLFIX_ENABLE_DICTIONARY
#define LLFIX_ENABLE_DICTIONARY
#endif

#ifndef ENABLE_SCALABLE_SERVER
#define ENABLE_SCALABLE_SERVER
#endif

#ifdef __linux__
//#define LLFIX_ENABLE_TCPDIRECT // Enable only in Linux and if there is Solarflare NIC available
#endif

#ifndef LLFIX_AUTOMATION
#define LLFIX_AUTOMATION
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <llfix/engine.h>

#include "automation_server.h"

#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/os/console.h>

#include "my_message_persister.h"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>

std::string get_file_content(const std::string& file_path)
{
    std::ifstream t(file_path);
    std::string content((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    return content;
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

void output(const std::string& message);
void process_validations(std::size_t, int);

int main(int argc, char* argv[])
{
    int mode = MODE1;

    if (argc > 1)
    {
        try
        {
            mode = std::stoi(argv[1]);
        }
        catch (...)
        {
            output("The argument should be one of : 1,2,3");
            return -1;
        }

        if (mode < 1 || mode>3)
        {
            output("The argument should be one of : 1,2,3");
            return -1;
        }
    }

    llfix::FileSystemUtilities::delete_directory_if_exists("clients");
    llfix::FileSystemUtilities::delete_file_if_exists("log.txt");

    std::string CONFIG_FILE = "config.cfg";
    llfix::Engine::on_start(CONFIG_FILE);

    AutomationServer server;
    server.set_mode(mode);

    bool creation_success = server.create("TEST_SERVER", CONFIG_FILE);

    if (creation_success == false)
    {
        output("Could not create server");
        return -1;
    }

    if (server.add_sessions_from(CONFIG_FILE) == false)
    {
        output("Failed to load server sessions");
        return -1;
    }

    server.specify_repeating_group("D", 453, 448, 447, 452);

    if (mode == MODE1) // triggers resend requests from clients while server in gap fill mode
    {
        server.set_replay_messages_on_incoming_resend_request_for_all_sessions(false);
    }
    else if (mode == MODE2) // triggers resend requests from clients while server in replay mode
    {
        server.set_replay_messages_on_incoming_resend_request_for_all_sessions(true);
    }

    std::unique_ptr<MyMessagePersister> custom_persister(new MyMessagePersister);

    //server.set_message_persist_plugin(custom_persister.get());

    server.start();

    llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Running in mode" + std::to_string(server.get_mode()) + "\n\n");

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press i to see the all session info\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press s to see the all session stats\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press v to run validations\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'i')
        {
            std::stringstream output;
            output << '\n' << server.get_all_sessions_display_text() << '\n';
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }
        else if (user_input[0] == 's')
        {
            std::stringstream output;
            output << '\n' << server.get_all_sessions_stats_display_text() << '\n';
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }
        else if (user_input[0] == 'v') // VALIDATIONS
        {
            process_validations(server.get_session_count(), mode);
        }
        else if (user_input[0] == 'q') // QUIT
        {
            break;
        }
    }

    llfix::Engine::stop_management_server();
    server.shutdown();
    llfix::Engine::shutdown();

    //////////////////////////////////////////////////////////////////////////////////////////////////

    #if _WIN32
    std::system("pause");
    #endif

    return 0;
}

void output(const std::string& message)
{
    std::cout << message << "\n";
}

void process_validation(std::size_t actual, std::size_t expected, const std::string& test_case)
{
    if (actual == expected)
    {
        output("Verification success : " + test_case);
    }
    else
    {
        output("Verification failure : " + test_case);
    }
}

void process_validations(std::size_t client_session_count, int mode)
{
    std::size_t new_order_per_client = 2000;
    std::string deserialiser_executable = "deserialiser.exe";

    #ifdef __linux__
    deserialiser_executable = "./deserialiser";
    #endif

    std::string SERIALISATIONS_TEXT_FILE = "messages.txt";
    llfix::FileSystemUtilities::delete_file_if_exists(SERIALISATIONS_TEXT_FILE);

    auto deserialiser_command = deserialiser_executable + " -i clients -o " + SERIALISATIONS_TEXT_FILE;
    auto res = std::system(deserialiser_command.c_str());

    if (res != 0)
    {
        output("Failed to deserialise messages");
        return;
    }

    // VERIFY INCOMING NEW ORDERS
    auto incoming_new_order_count = get_string_count(SERIALISATIONS_TEXT_FILE, "35=D");
    process_validation(incoming_new_order_count, (new_order_per_client * client_session_count), "incoming new orders");

    // VERIFY INCOMING REPLACE ORDERS
    auto incoming_replace_order_count = get_string_count(SERIALISATIONS_TEXT_FILE, "35=G");
    process_validation(incoming_replace_order_count, (new_order_per_client / 2 * client_session_count), "incoming replace orders");

    // VERIFY INCOMING CXL ORDERS
    auto incoming_cxl_order_count = get_string_count(SERIALISATIONS_TEXT_FILE, "35=F");
    process_validation(incoming_cxl_order_count, (new_order_per_client / 2 * client_session_count), "incoming cancel orders");

    // VERIFY INCOMING RESEND REQUESTS
    if (mode == MODE1 || mode == MODE2)
    {
        auto incoming_resend_request_count = get_string_count(SERIALISATIONS_TEXT_FILE, "35=2");
        process_validation(incoming_resend_request_count, client_session_count, "incoming resend requests");
    }

    // VERIFY OUTGOING FULL FILLS
    auto outgoing_full_fill_count = get_string_count(SERIALISATIONS_TEXT_FILE, "39=2");
    process_validation(outgoing_full_fill_count, (new_order_per_client / 2 * client_session_count), "outgoing full fills");

    // VERIFY OUTGOING ORDER ACKS
    auto outgoing_order_ack_count = get_string_count(SERIALISATIONS_TEXT_FILE, "39=0");
    process_validation(outgoing_order_ack_count, (new_order_per_client / 2 * client_session_count), "outgoing order acks");

    // VERIFY OUTGOING REPLACE ACKS
    auto outgoing_replace_ack_count = get_string_count(SERIALISATIONS_TEXT_FILE, "39=5");
    process_validation(outgoing_replace_ack_count, (new_order_per_client / 2 * client_session_count), "outgoing replace acks");

    // VERIFY OUTGOING CXL ACKS
    auto outgoing_cxl_ack_count = get_string_count(SERIALISATIONS_TEXT_FILE, "39=4");
    process_validation(outgoing_cxl_ack_count, (new_order_per_client / 2 * client_session_count), "outgoing cancel acks");

    // VERIFY OUTGOING GAP FILL MESSAGES
    if (mode == MODE1)
    {
        auto outgoing_gap_fill_count = get_string_count(SERIALISATIONS_TEXT_FILE, "123=Y");
        process_validation(outgoing_gap_fill_count, client_session_count, "outgoing gap fill messages");
    }

    // VERIFY OUTGOING REPLAY MESSAGES
    if (mode == MODE2)
    {
        auto outgoing_replay_message_count = get_string_count(SERIALISATIONS_TEXT_FILE, "43=Y");
        process_validation(outgoing_replay_message_count, client_session_count*6, "outgoing replay messages");
    }

    // VERIFY OUTGOING RESEND REQUESTS
    if (mode == MODE3)
    {
        auto outgoing_resend_request_count = get_string_count(SERIALISATIONS_TEXT_FILE, "35=2");
        process_validation(outgoing_resend_request_count, client_session_count, "outgoing resend requests");
    }

    // VERIFY 35=j MESSAGES
    auto reject_35_j_count = get_string_count(SERIALISATIONS_TEXT_FILE, "35=j");
    process_validation(reject_35_j_count, 0, "35=j reject messages");

    // VERIFY 35=3 MESSAGES
    auto reject_35_3_count = get_string_count(SERIALISATIONS_TEXT_FILE, "35=3");
    process_validation(reject_35_3_count, 0, "35=3 reject messages");

    // VERIFY LOGON MESSAGES
    auto logon_message_count = get_string_count(SERIALISATIONS_TEXT_FILE, "35=A");
    process_validation(logon_message_count, client_session_count*2, "35=A logon messages");
}