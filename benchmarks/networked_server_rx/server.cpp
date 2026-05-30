///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
//#define LLFIX_ENABLE_DICTIONARY
//#define ENABLE_SCALABLE_SERVER
#define LLFIX_ONLY_ERROR_AND_FATAL_LOGS // THAT ENSURES THAT OTHER LOG CALLS WON'T EVEN GET INTO THE BINARY
#define ENABLE_VALIDATIONS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARAMS
#if __linux__
#include <unistd.h> // geteuid for sudo check
#endif

#include <string>
static std::string CONFIG_FILE = "config.cfg";
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <llfix/engine.h>

#include "benchmark_server.h"

#include <llfix/core/utilities/configuration.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/os/console.h>

#include <llfix/core/cpu/simd_capabilities.h>

#include <iostream>
#include <sstream>

#include "../benchmark_utilities.h"

int main(int argc, char* argv[])
{
    #if __linux__
    if (geteuid() != 0)
    {
        std::cout << "You need to run this app with sudo." << std::endl;
        return -1;
    }
    #endif

    llfix::FileSystemUtilities::delete_directory_if_exists("messages");
    llfix::FileSystemUtilities::delete_directory_if_exists("clients_outgoing");
    llfix::FileSystemUtilities::delete_directory_if_exists("clients_seq_stores");
    llfix::FileSystemUtilities::delete_file_if_exists("log.txt");

    llfix::Engine::on_start(CONFIG_FILE);

    BenchmarkServer server;

    bool creation_success = server.create("TEST_SERVER", CONFIG_FILE);

    if (creation_success == false)
    {
        std::cout << "Could not create server" << std::endl;
        return -1;
    }

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

    auto serialised_file_max_size = config.get_int_value("serialised_file_max_size", 67108864);

    for (int i = 0; i < client_count; i++)
    {
        std::string session_name = "SESSION" + std::to_string(i+1);

        llfix::FixSessionSettings session_settings;

        session_settings.begin_string = "FIXT.1.1";
        session_settings.default_app_ver_id = "7";
        session_settings.sender_comp_id = "EXECUTOR";
        session_settings.target_comp_id = "CLIENT" + std::to_string(i + 1);
        session_settings.heartbeat_interval_in_nanoseconds = 10'000'000'000;

        session_settings.throttle_window_in_milliseconds = 1;
        session_settings.throttle_limit = 0;

        if(llfix::SIMDCapabilities::instance().supports_simd_avx2())
        {
            session_settings.enable_simd_avx2 = true;
        }

        #ifdef ENABLE_VALIDATIONS
        session_settings.validate_repeating_groups = true;
        session_settings.validations_enabled = true;
        session_settings.max_allowed_message_age_seconds = 60;
        #ifdef LLFIX_ENABLE_DICTIONARY
        session_settings.dictionary_validations_enabled = true;
        #endif
        #else
        session_settings.validate_repeating_groups = false;
        session_settings.validations_enabled = false;
        session_settings.max_allowed_message_age_seconds = 0;
        #ifdef LLFIX_ENABLE_DICTIONARY
        session_settings.dictionary_validations_enabled = false;
        #endif
        #endif

        //////////////////////////////////////////////
        #ifdef LLFIX_ENABLE_DICTIONARY
        session_settings.application_dictionary_path = "../../tests/dictionaries/FIX50SP2.xml";
        session_settings.transport_dictionary_path = "../../tests/dictionaries/FIXT11.xml";
        #endif

        session_settings.max_serialised_file_size = serialised_file_max_size;
        session_settings.sequence_store_file_path = "clients_seq_stores/client" + std::to_string(i + 1) + "/sequence.store";               // clients/client1/sequence.store
        session_settings.incoming_message_serialisation_path = "messages/client" + std::to_string(i + 1) + "/messages_incoming"; // clients/client1/messages_incoming
        session_settings.outgoing_message_serialisation_path = "clients_outgoing/client" + std::to_string(i + 1) + "/messages_outgoing"; // clients/client1/messages_outgoing

        session_settings.initialise_derived_settings();

        if (server.add_session(session_name, session_settings) == false)
        {
            std::cout << "Failed to add client session " << std::to_string(i + 1) << "\n";
            return -1;
        }
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////
    auto cpu_frequency = ProcessorUtilities::get_current_cpu_frequency_hertz();

    std::cout << "CPU frequency " << cpu_frequency << " hertz\n";

    #ifdef __linux__
    std::cout << "CPU isolation config : " << LinuxInfo::get_cpu_isolation_info() << "\n";
    #endif

    std::cout << "AVX2 available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx2() ? "true" : "false") << "\n";
    std::cout << "AVX512F available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512f() ? "true" : "false") << "\n";
    std::cout << "AVX512BW available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512bw() ? "true" : "false") << "\n";
    std::cout << "AVX512FBW available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512fbw() ? "true" : "false") << "\n";

    server.specify_repeating_group("D", 453, 448, 447, 452);

    server.start();

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press i to see the all session info\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'i')
        {
            std::stringstream output;
            output << '\n' << server.get_all_sessions_display_text() << '\n';
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }
        else if (user_input[0] == 'q') // QUIT
        {
            break;
        }
    }

    server.shutdown();

    //////////////////////////////////////////////////////////////////////////////////////////////////

    #if _WIN32
    std::system("pause");
    #endif

    return 0;
}