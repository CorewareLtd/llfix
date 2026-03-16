#ifndef LLFIX_ENABLE_BINARY_FIELDS
#define LLFIX_ENABLE_BINARY_FIELDS
#endif

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
        bool send_heartbeat(llfix::FixSession* session, llfix::FixString* test_request_id) override
        {
            std::cout << "Sending heartbeat\n";
            session->build_heartbeat_message(outgoing_message_instance(session), test_request_id);
            return send_outgoing_message(session, outgoing_message_instance(session));
        }

        void on_custom_message(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            std::cout << "Custom message : " << message->to_string() << "\n";
            send_custom_message(session);
        }

        void send_custom_message(llfix::FixSession* session)
        {
            auto msg = outgoing_message_instance(session);
            msg->set_msg_type("YX");

            msg->set_tag<llfix::FixMessageComponent::HEADER>(347, "UTF-8");

            const char japanese[] = u8"こんにちは、世界！"; // NEED TO SPECIFY u8 for utf8 , otherwise its sizeof is deducted as 10, but actually it is 28 since it is unicode
            msg->set_tag(354, sizeof(japanese));
            msg->set_binary_tag(355, japanese, sizeof(japanese));

            send_outgoing_message(session, msg);
        }

        void on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        { 
            LLFIX_UNUSED(session);
            std::cout << "Logon message : " << message->to_string() << "\n";
        }
};

int main()
{
    llfix::FileSystemUtilities::delete_directory_if_exists("client1");
    llfix::FileSystemUtilities::delete_file_if_exists("server_log.txt");

    std::string config_file = "server_config.cfg";

    /////////////////////////////////////////////////////////////////////////////
    llfix::Engine::on_start(config_file);
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

    server.specify_binary_field("A", 95, 96);

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