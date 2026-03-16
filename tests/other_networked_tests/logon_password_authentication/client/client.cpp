#include <llfix/engine.h>

#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/tcp_connector.h>
#include <llfix/fix_client.h>

#include <llfix/core/utilities/filesystem_utilities.h>

#include <iostream>

class SampleClient : public llfix::FixClient<llfix::TCPConnector>
{
    public:

        void on_execution_report(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(message);
        }
};

int main(int argc, char* argv[])
{
    llfix::FileSystemUtilities::delete_directory_if_exists("messages_outgoing");
    llfix::FileSystemUtilities::delete_directory_if_exists("messages_incoming");
    llfix::FileSystemUtilities::delete_file_if_exists("sequence.store");
    llfix::FileSystemUtilities::delete_file_if_exists("log.txt");

    std::string config_file = "config.cfg";

    if (argc > 1)
    {
        config_file = argv[1];
    }

    llfix::Engine::on_start(config_file);

    SampleClient client;

    if (client.create(config_file, "EXAMPLE_CLIENT", config_file, "EXAMPLE_SESSION") == false)
    {
        std::cout << "Fix client creation failed. Check the logs.\n";
        return -1;
    }

    bool connection_success = client.connect();

    if (connection_success == false)
    {
        std::cout << "Connection to server failed. Please start the server before runnning this process." << std::endl;
        return -1;
    }

    while(true)
    {
        client.process_incoming_messages();

        auto session_state = client.get_session()->get_state();

        if( session_state == llfix::SessionState::LOGGED_ON )
        {
            std::cout << "Logged on...\n";
            break;
        }
    }

    std::cout << "Ctrl c to quit...\n";

    while (true)
    {
        client.process();
    }

    return 0;
}