///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define LLFIX_ENABLE_DICTIONARY
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <iostream>

#include <llfix/engine.h>
#include <llfix/fix_client_settings.h>
#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/tcp_connector.h>
#include <llfix/fix_client.h>

#include <llfix/core/utilities/filesystem_utilities.h>

static std::size_t              THROTTLE_LIMIT = 100000;
static constexpr uint64_t       THROTTLE_WINDOW_IN_MILLISECONDS = 1000;
static constexpr std::size_t    HEARTBEAT_INTERVAL_SECONDS = 30;

class SampleClient : public llfix::FixClient<llfix::TCPConnector>
{
    public:

        SampleClient()
        {
            specify_repeating_group("8", 453, 448, 447, 452);
            specify_repeating_group("8", 600, 601, 603, 603);
        }

        void on_execution_report(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(message);
        }
};

int main()
{
    llfix::FileSystemUtilities::delete_directory_if_exists("messages_outgoing");
    llfix::FileSystemUtilities::delete_directory_if_exists("messages_incoming");
    llfix::FileSystemUtilities::delete_file_if_exists("sequence.store");
    llfix::FileSystemUtilities::delete_file_if_exists("log.txt");

    llfix::Engine::on_start("config.cfg");

    llfix::FixClientSettings client_settings;

    client_settings.primary_port = 5001;
    

    #ifdef LLFIX_ENABLE_TCPDIRECT
    client_settings.primary_address = "192.168.10.100";
    client_settings.nic_name = "ens3f0";
    client_settings.nic_address = "192.168.10.103";
    #else
    client_settings.primary_address = "127.0.0.1";
    #endif

    llfix::FixSessionSettings session_settings;

    session_settings.set_heartbeat_interval_in_nanoseconds((int)HEARTBEAT_INTERVAL_SECONDS);
    session_settings.throttle_window_in_milliseconds = THROTTLE_WINDOW_IN_MILLISECONDS;
    session_settings.throttle_limit = THROTTLE_LIMIT;

    session_settings.begin_string = "FIXT.1.1";
    session_settings.sender_comp_id = "CLIENT1";
    session_settings.target_comp_id = "EXECUTOR";
    session_settings.replay_messages_on_incoming_resend_request = true;
    
    session_settings.default_app_ver_id = 7;

    session_settings.max_serialised_file_size = 4096;

    session_settings.validations_enabled = true;
    session_settings.validate_repeating_groups = true;
    session_settings.max_allowed_message_age_seconds = 60;

    session_settings.application_dictionary_path = "../../dictionaries/FIX50SP2_modified.xml";
    session_settings.transport_dictionary_path   = "../../dictionaries/FIXT11.xml";
    
    session_settings.initialise_derived_settings();

    SampleClient client;

    bool creation_success = client.create("EXAMPLE_CLIENT", client_settings, "EXAMPLE_SESSION", session_settings);

    if (creation_success == false)
    {
        std::cout << "Could not create client" << std::endl;
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