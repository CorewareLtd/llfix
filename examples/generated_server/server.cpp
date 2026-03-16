#include <llfix/engine.h>
#include "MyServer.h"

#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>

#include <string>
#include <iostream>

int main(int argc, char* argv[])
{
    std::string config_file = "config.cfg";

    /////////////////////////////////////////////////////////////////////////////
    llfix::Engine::on_start(config_file);
    custom::MyServer server;

    if (server.create("EXAMPLE_SERVER", config_file) == false)
    {
        std::cout << "Failed to create the FIX server. Check the logs\n";
        return -1;
    }

    if (server.add_sessions_from(config_file) == false)
    {
        std::cout << "Failed to load sessions from " << config_file << ". Check the logs\n";
        return -1;
    }

    llfix::Engine::get_management_server().register_server(&server);

    server.start();

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press s to display session details\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            break;
        }
        else if (user_input[0] == 's') // SESSIONS
        {
            const std::string session_name = "SESSION1";
            auto session = server.get_session(session_name);

            std::string session_data = session->get_display_text();
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, session_data + "\n");
        }
    }

    llfix::Engine::stop_management_server();
    server.shutdown();
    llfix::Engine::shutdown();

    return 0;
}