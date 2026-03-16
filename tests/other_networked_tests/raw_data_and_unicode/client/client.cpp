#ifndef LLFIX_ENABLE_BINARY_FIELDS
#define LLFIX_ENABLE_BINARY_FIELDS
#endif

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

        void on_logon_response(const llfix::IncomingFixMessage* message)
        {
            LLFIX_UNUSED(message);
            std::cout << "Logged on\n";
            send_custom_message();
        }

        bool send_client_heartbeat(llfix::FixString* test_request_id) override
        {
            std::cout << "Sending heartbeat\n";
            get_session()->build_heartbeat_message(outgoing_message_instance(), test_request_id);
            return send_outgoing_message(outgoing_message_instance());
        }

        void on_custom_message_type(const llfix::IncomingFixMessage* message)  override
        {
            std::cout << "Custom message : " << message->to_string() << "\n";
        }

        void send_custom_message()
        {
            auto message = outgoing_message_instance();
            message->set_msg_type("XT");
            send_outgoing_message(message);
        }

        bool send_logon_request() override
        {
            auto logon_request = outgoing_message_instance();
            logon_request->set_msg_type(llfix::FixConstants::MSG_TYPE_LOGON);

            // TAG 98 ENCRYPTION METHOD
            logon_request->set_tag(llfix::FixConstants::TAG_ENCRYPT_METHOD, 0);

            // TAG 108 HEARTBEAT INTERVAL
            logon_request->set_tag(llfix::FixConstants::TAG_HEART_BT_INT, static_cast<uint32_t>(get_session()->get_heartbeart_interval_in_nanoseconds() / 1'000'000'000));

            // TAG 1137 DEFAULT APP VER ID
            auto default_app_ver_id = get_session()->get_default_app_ver_id();

            if (default_app_ver_id.length() > 0)
            {
                logon_request->set_tag(llfix::FixConstants::TAG_DEFAULT_APPL_VER_ID, default_app_ver_id);
            }

            // TAG 141 RESET SEQ NOS
            if (get_session()->logon_reset_sequence_numbers_flag())
            {
                get_session()->get_sequence_store()->reset_numbers();
                logon_request->set_tag(llfix::FixConstants::TAG_RESET_SEQ_NUM_FLAG, llfix::FixConstants::FIX_BOOLEAN_TRUE);
            }

            // TAG 95 AND 96
            char binary_data[4];
            binary_data[0] = 'x';
            binary_data[1] = 'y';
            binary_data[2] = ((char)(1)); // SOH
            binary_data[3] = 'z';

            logon_request->set_tag(95, sizeof(binary_data));
            logon_request->set_binary_tag(96, binary_data, 4);

            return send_outgoing_message(logon_request);
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

    client.specify_binary_field("YX", 354, 355);

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