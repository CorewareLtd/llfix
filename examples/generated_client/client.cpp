#include <llfix/engine.h>
#include "MyClient.h"

#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>

#include <string>
#include <iostream>

int main(int argc, char* argv[])
{
    std::string config_file = "config.cfg";

    if (argc > 1)
    {
        config_file = argv[1];
    }

    llfix::Engine::on_start(config_file);

    custom::MyClient client;

    if (client.create(config_file, "EXAMPLE_CLIENT", config_file, "EXAMPLE_SESSION") == false)
    {
        std::cout << "Fix client creation failed. Check the logs.\n";
        return -1;
    }

    bool connection_success = client.connect();

    if (connection_success == false)
    {
        std::cout << "Connection to server failed. Please start the server before runnning this process and check the logs." << std::endl;
        return -1;
    }

    llfix::Engine::get_management_server().register_client(&client);

    if(client.start() == false)
    {
        std::cout << "Fix client thread creation failed.\n";
        return -1;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::string user_input;

    while (true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press s to see session details\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            break;
        }
        else if (user_input[0] == 's')
        {
            std::string session_data = client.get_session()->get_display_text();
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, session_data + "\n");
        }
    }

    llfix::Engine::stop_management_server();
    client.shutdown(true);
    llfix::Engine::shutdown();

    return 0;
}