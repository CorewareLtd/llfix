///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
//#define LLFIX_ENABLE_OPENSSL
#define ENABLE_SCALABLE_SERVER
#define LLFIX_ENABLE_DICTIONARY
//#define ENABLE_MESSAGE_REPLAY
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <llfix/engine.h>

#include "test_server.h"

#include <llfix/core/utilities/configuration.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/os/console.h>

#include <string>
#include <iostream>
#include <sstream>

static std::string CONFIG_FILE = "config.cfg";

int main()
{
    llfix::Engine::on_start(CONFIG_FILE);

    TestServer server;

    bool creation_success = server.create("TEST_SERVER", CONFIG_FILE);

    if (creation_success == false)
    {
        std::cout << "Could not create server" << std::endl;
        return -1;
    }

    llfix::Engine::get_management_server().register_server(&server);

    /////////////////////////////////////////////////////////////////////////////////////////////////
    // ADDING SESSIONS
    std::string config_load_error;
    llfix::Configuration config;

    if (config.load_from_file(CONFIG_FILE, config_load_error) == false)
    {
        std::cout << "Failed to load config file" << std::endl;
        return -1;
    }

    auto client_count = config.get_int_value("client_count", 4);

    if (client_count <= 0)
    {
        std::cout << "client count config should be a positive number" << std::endl;
        return -1;
    }

    auto dictionary_path = config.get_string_value("dictionary_path");

    #ifdef LLFIX_ENABLE_DICTIONARY
    if (dictionary_path.empty())
    {
        std::cout << "You must specify dictionary_path" << std::endl;
        return -1;
    }
    #endif

    auto serialised_file_max_size = config.get_int_value("serialised_file_max_size", 67108864);

    for (int i = 0; i < client_count; i++)
    {
        std::string session_name = "SESSION" + std::to_string(i+1);

        llfix::FixSessionSettings session_settings;

        session_settings.begin_string = "FIX.4.4";
        session_settings.sender_comp_id = "EXECUTOR";
        session_settings.target_comp_id = "CLIENT" + std::to_string(i + 1);
        session_settings.heartbeat_interval_in_nanoseconds = 10'000'000'000;

        session_settings.throttle_window_in_milliseconds = 1;
        session_settings.throttle_limit = 0;

        session_settings.outgoing_test_request_interval_multiplier = 5;

        //////////////////////////////////////////////
        session_settings.max_serialised_file_size = serialised_file_max_size;
        session_settings.sequence_store_file_path = "clients/client" + std::to_string(i + 1) + "/sequence.store";               // clients/client1/sequence.store
        session_settings.incoming_message_serialisation_path = "clients/client" + std::to_string(i + 1) + "/messages_incoming"; // clients/client1/messages_incoming
        session_settings.outgoing_message_serialisation_path = "clients/client" + std::to_string(i + 1) + "/messages_outgoing"; // clients/client1/messages_outgoing

        session_settings.initialise_derived_settings();

        #ifdef LLFIX_ENABLE_DICTIONARY
        session_settings.application_dictionary_path = dictionary_path;
        #endif

        #ifdef ENABLE_MESSAGE_REPLAY
        session_settings.replay_messages_on_incoming_resend_request = true;
        #endif

        if (server.add_session(session_name, session_settings) == false)
        {
            std::cout << "Failed to add client session " << std::to_string(i + 1) << "\n";
            return -1;
        }
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////

    server.init_stats_per_session();
    server.specify_repeating_group("D", 453, 448, 447, 452);

    if(server.start() == false)
    {
        std::cout << "Failed to start server\n";
        return -1;
    }

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press i to see the all session info\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press s to see the all session stats\n");
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