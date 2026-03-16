///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define LLFIX_ONLY_ERROR_AND_FATAL_LOGS // THAT ENSURES THAT OTHER LOG CALLS WON'T EVEN GET INTO THE BINARY

#ifndef LLFIX_BENCHMARK
#define LLFIX_BENCHMARK
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARAMS
#include <cstddef>

static constexpr int CPU_CORE_INDEX_TO_PIN = 2;
static constexpr std::size_t EXEC_REPORT_COUNT = 1'000'000;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <llfix/common.h>
#include "../benchmark_utilities.h"
#include <llfix/engine.h>

#include <llfix/core/compiler/builtin_functions.h>
#include <llfix/core/os/thread_utilities.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/utilities/allocator.h>

#include <cstdint>
#include <string>
#include <iostream>

#include "dummy_client.h"

using namespace std;

int main(int argc, char* argv[])
{
    llfix::FileSystemUtilities::delete_directory_if_exists("messages_outgoing");
    llfix::FileSystemUtilities::delete_directory_if_exists("messages_incoming");
    llfix::FileSystemUtilities::delete_file_if_exists("sequence.store");

    #ifdef __linux__
    std::cout << "CPU isolation config : " << LinuxInfo::get_cpu_isolation_info() << "\n";
    #endif

    if(CPU_CORE_INDEX_TO_PIN+1 > llfix::ThreadUtilities::get_number_of_physical_cores())
    {
        std::cout << "Please change CPU_CORE_INDEX_TO_PIN and re build as this host doesn't have that core index." << std::endl;
        return -1;
    }

    ProcessorUtilities::pin_calling_thread_to_cpu_core(CPU_CORE_INDEX_TO_PIN);
    std::cout << "Pinned to CPU core " << CPU_CORE_INDEX_TO_PIN << "\n";

    //////////////////////////////////////////////////////////////////////////
    llfix::FixClientSettings client_settings;

    client_settings.primary_port = 5001;
    client_settings.primary_address = "127.0.0.1";

    llfix::FixSessionSettings session_settings;

    session_settings.begin_string = "FIX.4.2";
    session_settings.sender_comp_id = "TRADER";
    session_settings.target_comp_id = "EXECUTOR";
    session_settings.default_app_ver_id = "9";

    session_settings.throttle_window_in_milliseconds=1;
    session_settings.throttle_limit=2000;

    session_settings.max_serialised_file_size = 671088640; //640 MB

    session_settings.initialise_derived_settings();

    std::string message_buffer = "8=FIX.4.2|9=161|35=8|34=1|49=EXECUTOR|52=20230901-12:30:45.000|56=TRADER|17=12345|20=0|37=ABC123|39=1|11=1|150=1|32=1|31=100|453=2|448=PARTY1|447=D|452=1|448=PARTY2|447=D|452=3|10=255|";
    for (char& ch : message_buffer)
    {
        if (ch == '|')
        {
            ch = (char)(1);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    {
        DummyClient client;

        bool creation_success = client.create("EXAMPLE_CLIENT", client_settings, "EXAMPLE_SESSION", session_settings);

        if (creation_success == false)
        {
            std::cout << "Could not create client" << std::endl;
            return -1;
        }

        client.get_session()->set_validations_enabled(false);

        {
            BENCHMARK_BEGIN(EXEC_REPORT_COUNT)
            client.process_rx_buffer(const_cast<char *>(message_buffer.c_str()), message_buffer.length());
            client.get_session()->get_sequence_store()->set_incoming_seq_no(0);
            BENCHMARK_END()
            report.print("FIX exec report processing without validations");
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    {
        DummyClient client;

        bool creation_success = client.create("EXAMPLE_CLIENT", client_settings, "EXAMPLE_SESSION", session_settings);

        if (creation_success == false)
        {
            std::cout << "Could not create client" << std::endl;
            return -1;
        }

        client.get_session()->set_validations_enabled(true);
        client.get_session()->set_validate_repeating_groups_enabled(true);

        {
            BENCHMARK_BEGIN(EXEC_REPORT_COUNT)
            client.process_rx_buffer(const_cast<char *>(message_buffer.c_str()), message_buffer.length());
            client.get_session()->get_sequence_store()->set_incoming_seq_no(0);
            BENCHMARK_END()
            report.print("FIX exec report processing with validations");
        }
    }

    #if _WIN32
    std::system("pause");
    #endif

    return 0;
}