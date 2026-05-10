#include <atomic>
#include <cstdint>
#include <string>
#include <iostream>
#include <thread>

#include <llfix/core/os/console.h>
#include <llfix/core/utilities/configuration.h>
#include <llfix/core/utilities/filesystem_utilities.h>

#include "serialisation_path_reader.h"
#include "otlp_client.h"

int main(int argc, char**argv)
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CONFIGS
    std::string config_file_path = "config.cfg";

    if (argc > 1)
    {
        config_file_path = argv[1];
    }

    llfix::Configuration config;
    std::string config_load_error;

    if (config.load_from_file(config_file_path, config_load_error) == false)
    {
        std::cerr << "Failed to load configuration from file: " << config_load_error << "\n";
        return 1;
    }

    std::string incoming_session_name = config.get_string_value("incoming_session_name");
    std::string incoming_serialisation_path = config.get_string_value("incoming_serialisation_path");
    int incoming_max_serialisation_file_size = config.get_int_value("incoming_max_serialised_file_size", 67108864);

    std::string outgoing_serialisation_path = config.get_string_value("outgoing_serialisation_path");
    int outgoing_max_serialisation_file_size = config.get_int_value("outgoing_max_serialised_file_size", 67108864);
    std::string outgoing_session_name = config.get_string_value("outgoing_session_name");

    std::string otlp_application_name = config.get_string_value("otlp_application_name");
    std::string otlp_endpoint = config.get_string_value("otlp_endpoint");

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INCOMING MESSAGES
    if (llfix::FileSystemUtilities::does_path_exist(incoming_serialisation_path) == false)
    {
        std::cerr << "Path doesn't exist : " << incoming_serialisation_path << "\n";
        return 2;
    }

    if (incoming_max_serialisation_file_size == 0)
    {
        std::cerr << "incoming_max_serialisation_file_size should be a positive number\n";
        return 3;
    }

    llfix::SerialisationPathReader incoming_reader;

    if (incoming_reader.initialise(incoming_serialisation_path, incoming_max_serialisation_file_size) == false)
    {
        std::cerr << "Failed to process path " << incoming_serialisation_path << "\n";
        return 4;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // OUTGOING MESSAGES
    if (llfix::FileSystemUtilities::does_path_exist(outgoing_serialisation_path) == false)
    {
        std::cerr << "Path doesn't exist : " << outgoing_serialisation_path << "\n";
        return 5;
    }

    if (outgoing_max_serialisation_file_size == 0)
    {
        std::cerr << "outgoing_max_serialisation_file_size should be a positive number\n";
        return 6;
    }

    llfix::SerialisationPathReader outgoing_reader;

    if (outgoing_reader.initialise(outgoing_serialisation_path, outgoing_max_serialisation_file_size) == false)
    {
        std::cerr << "Failed to process path " << outgoing_serialisation_path << "\n";
        return 7;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // OTLP CLIENT
    OTLPClient otlp_client(otlp_application_name, otlp_endpoint);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // APPLICATION THREAD
    std::atomic<bool> is_exiting = false;
    uint64_t incoming_latest_post_message_index = 0;
    uint64_t outgoing_latest_post_message_index = 0;

    auto observer_thread_fn = [&]()
        {
            while (true)
            {
                if (is_exiting.load() == true)
                {
                    break;
                }

                auto incoming_latest_message_index = incoming_reader.get_latest_message_index();
                auto outgoing_latest_message_index = outgoing_reader.get_latest_message_index();

                if (incoming_latest_message_index > incoming_latest_post_message_index || outgoing_latest_message_index > outgoing_latest_post_message_index)
                {
                    otlp_client.post(incoming_session_name, incoming_latest_message_index, outgoing_session_name, outgoing_latest_message_index);
                    // std::cout << "Incoming session index:" << incoming_latest_message_index << ", Outgoing session index: " << outgoing_latest_message_index << "\n";

                    incoming_latest_post_message_index = incoming_latest_message_index;
                    outgoing_latest_post_message_index = outgoing_latest_message_index;
                }
            }
        };

    std::thread observer_thread(observer_thread_fn);
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // APPLICATION LOOP
    std::string user_input;

    while (true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            is_exiting.store(true);
            break;
        }
    }

    observer_thread.join();

    return 0;
}