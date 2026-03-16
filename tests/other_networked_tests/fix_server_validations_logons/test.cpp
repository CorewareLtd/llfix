#include <string>
#include <iostream>

#include <llfix/engine.h>

#include <llfix/core/utilities/tcp_reactor.h>
#include <llfix/fix_server.h>

#include <llfix/core/os/console.h>
#include <llfix/core/utilities/filesystem_utilities.h>

class TestServer : public llfix::FixServer<llfix::TcpReactor<>>
{
    public:
    private:
};

int main()
{
    llfix::FileSystemUtilities::delete_directory_if_exists("client1");
    llfix::FileSystemUtilities::delete_directory_if_exists("client2");
    llfix::FileSystemUtilities::delete_directory_if_exists("client3");
    llfix::FileSystemUtilities::delete_directory_if_exists("client4");
    llfix::FileSystemUtilities::delete_file_if_exists("log.txt");

    llfix::Engine::on_start("config.cfg");

    std::string config_file = "config.cfg";
    TestServer server;

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

    server.specify_repeating_group("D", 453, 448, 447, 452);

    llfix::Engine::get_management_server().register_server(&server);

    server.start();

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            break;
        }
    }

    server.shutdown();

    return 0;
}