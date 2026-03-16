#include <string>
#include <iostream>

#include <llfix/engine.h>

#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/tcp_reactor.h>
#include <llfix/fix_server.h>

#include <llfix/core/os/console.h>
#include <llfix/core/utilities/filesystem_utilities.h>

class TestServer : public llfix::FixServer<llfix::TcpReactor<>>
{
    public:
        virtual bool authenticate_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(session);

            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Incoming logon req : " + message->to_string() + "\n");

            // Check username
            if (message->has_tag(553) == false)
            {
                std::cout << "authentication failure : no t553\n";
                return false;
            }

            if (message->get_tag_value_as<std::string>(553) != m_username)
            {
                std::cout << "authentication failure : invalid username\n";
                return false;
            }

            // Check password
            if (message->has_tag(554) == false)
            {
                std::cout << "authentication failure : no t554\n";
                return false;
            }

            if (message->get_tag_value_as<std::string>(554) != m_password)
            {
                std::cout << "authentication failure : invalid username\n";
                return false;
            }

            // Process new password if it is there
            if (message->has_tag(925))
            {
                m_password = message->get_tag_value_as<std::string>(925);
                std::cout << "new password accepted\n";
            }

            std::cout << "authentication success\n";

            return true;
        }

        void set_username(const std::string& username)
        {
            m_username = username;
        }

        void set_password(const std::string& password)
        {
            m_password = password;
        }

    private:
        std::string m_username;
        std::string m_password;
};

int main()
{
    llfix::FileSystemUtilities::delete_directory_if_exists("client1");
    llfix::FileSystemUtilities::delete_file_if_exists("server_log.txt");

    std::string config_file = "server_config.cfg";

    /////////////////////////////////////////////////////////////////////////////
    // GET CONFIGURED USERNAME AND PASSWORD
    std::string config_load_error;
    llfix::Configuration config;

    if (!config.load_from_file(config_file, config_load_error))
    {
        std::cout << "Failed to load the config file.\n";
        return -1;
    }

    auto username = config.get_string_value("username");
    auto password = config.get_string_value("password");

    if (!username.length() || !password.length())
    {
        std::cout << "Please configure user name and password.\n";
        return -1;
    }
    /////////////////////////////////////////////////////////////////////////////
    llfix::Engine::on_start(config_file);
    TestServer server;

    server.set_username(username);
    server.set_password(password);

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