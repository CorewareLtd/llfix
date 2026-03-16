///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define LLFIX_ONLY_ERROR_AND_FATAL_LOGS // THAT ENSURES THAT OTHER LOG CALLS WON'T EVEN GET INTO THE BINARY
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARAMS
#include <cstddef>

static constexpr int CPU_CORE_INDEX_TO_PIN = 2;
static constexpr std::size_t NEW_ORDER_COUNT = 1'000'000;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <llfix/engine.h>
#include "../benchmark_utilities.h"

#include <llfix/core/compiler/builtin_functions.h>
#include <llfix/core/os/thread_utilities.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/fix_session.h>
#include <llfix/outgoing_fix_message.h>
#include <llfix/core/utilities/allocator.h>

#include <llfix/core/cpu/simd_capabilities.h>

#include <cstdint>
#include <string>
#include <iostream>

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

    std::cout << "AVX2 available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx2() ? "true" : "false") << "\n";
    std::cout << "AVX512F available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512f() ? "true" : "false") << "\n";
    std::cout << "AVX512BW available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512bw() ? "true" : "false") << "\n";
    std::cout << "AVX512FBW available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512fbw() ? "true" : "false") << "\n";

    //////////////////////////////////////////////////////////////////////////
    llfix::FixSession session;
    session.set_compid("CLIENT1");
    session.set_target_compid("SERVER");
    session.set_begin_string("FIXT1.1");

    uint32_t sequence_no = 0;
    uint32_t order_id = 0;
    const std::string symbol = "NOKIA.HE";

    int qty = 100;

    constexpr std::size_t BUFFER_SIZE = 2048;
    char buffer[BUFFER_SIZE];
    llfix_builtin_memset(buffer, 0 , BUFFER_SIZE);

    llfix::OutgoingFixMessage outgoing_fix_message;

    session.settings()->initialise_derived_settings();

    if (llfix::SIMDCapabilities::instance().supports_simd_avx2())
    {
        session.set_enable_simd_avx2(true);
    }

    if(outgoing_fix_message.initialise(session.settings(), session.get_sequence_store()) == false)
    {
        std::cout << "OutgoingFixMessage creation failed\n";
        return -1;
    }

    outgoing_fix_message.set_additional_static_header_tag(50, "sender_sub_id");
    outgoing_fix_message.set_additional_static_header_tag(57, "receiver_sub_id");

    auto serialiser = session.get_outgoing_message_serialiser();

    if (serialiser->initialise(session.settings()->outgoing_message_serialisation_path, session.settings()->max_serialised_file_size*5, session.settings()->replay_messages_on_incoming_resend_request, session.settings()->replay_message_cache_initial_size) == false)
    {
        std::cout << "Failed to initialise serialiser\n";
        return -1;
    }

    // ENCODING ( NEW ORDER ) WITHOUT SERIALISATION
    {
        BENCHMARK_BEGIN(NEW_ORDER_COUNT)
        {
            order_id++;
            sequence_no++;

            outgoing_fix_message.set_msg_type('D');
            outgoing_fix_message.set_timestamp_tag(60);
            outgoing_fix_message.set_tag(55, symbol);
            outgoing_fix_message.set_tag(11, order_id);

            outgoing_fix_message.set_tag(54, '1');

            outgoing_fix_message.set_tag(38, qty);
            outgoing_fix_message.set_tag(44, 444);
            outgoing_fix_message.set_tag(40, '2');
            outgoing_fix_message.set_tag(59, '0');

            outgoing_fix_message.set_tag(453, 2);
            outgoing_fix_message.set_tag(448, "PARTY1");
            outgoing_fix_message.set_tag(447, 'D');
            outgoing_fix_message.set_tag(452, 1);
            outgoing_fix_message.set_tag(448, "PARTY2");
            outgoing_fix_message.set_tag(447, 'D');
            outgoing_fix_message.set_tag(452, 3);

            std::size_t encoded_length = 0;
            outgoing_fix_message.encode(buffer, BUFFER_SIZE, sequence_no, encoded_length);
        }

        BENCHMARK_END()
        report.print("FIX New order encoding without serialisation");
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    // ENCODING ( NEW ORDER ) WITH SERIALISATION
    {
        BENCHMARK_BEGIN(NEW_ORDER_COUNT)
        {
            order_id++;
            sequence_no++;

            outgoing_fix_message.set_msg_type('D');
            outgoing_fix_message.set_timestamp_tag(60);
            outgoing_fix_message.set_tag(55, symbol);
            outgoing_fix_message.set_tag(11, order_id);

            outgoing_fix_message.set_tag(54, '1');

            outgoing_fix_message.set_tag(38, qty);
            outgoing_fix_message.set_tag(44, 444);
            outgoing_fix_message.set_tag(40, '2');
            outgoing_fix_message.set_tag(59, '0');

            outgoing_fix_message.set_tag(453, 2);
            outgoing_fix_message.set_tag(448, "PARTY1");
            outgoing_fix_message.set_tag(447, 'D');
            outgoing_fix_message.set_tag(452, 1);
            outgoing_fix_message.set_tag(448, "PARTY2");
            outgoing_fix_message.set_tag(447, 'D');
            outgoing_fix_message.set_tag(452, 3);

            std::size_t encoded_length = 0;
            outgoing_fix_message.encode(buffer, BUFFER_SIZE, sequence_no, encoded_length);

            serialiser->write(buffer, encoded_length, true);
        }

        BENCHMARK_END()
        report.print("FIX New order encoding with serialisation");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    #if _WIN32
    std::system("pause");
    #endif

    return 0;
}