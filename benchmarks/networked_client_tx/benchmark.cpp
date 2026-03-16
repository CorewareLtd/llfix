///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define LLFIX_ONLY_ERROR_AND_FATAL_LOGS // THAT ENSURES THAT OTHER LOG CALLS WON'T EVEN GET INTO THE BINARY
#ifdef __linux__
//#define LLFIX_ENABLE_TCPDIRECT // Enable only in Linux and if there is Solarflare NIC available
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARAMS
#include <cstddef>

#if __linux__
#include <unistd.h> // geteuid for sudo check
#endif

static constexpr std::size_t    ITERATION_COUNT_NEW_ORDER = 1'000'000;
static constexpr std::size_t    ANTISTALL_MOCK_ORDER_COUNT = 1000;
static constexpr int            CPU_CORE_INDEX_TO_PIN = 10;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <llfix/engine.h>
#include <llfix/core/utilities/allocator.h>

#include "../benchmark_utilities.h"

#include <llfix/fix_client.h>
#include <llfix/fix_client_settings.h>

#include "order.h"
#include "sample_client.h"

#include <llfix/core/compiler/builtin_functions.h>
#include <llfix/core/os/thread_utilities.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/utilities/object_cache.h>

#include <llfix/core/cpu/simd_capabilities.h>

#include <cstdint>

int main(int argc, char* argv[])
{
    #if __linux__
    if (geteuid() != 0)
    {
        std::cout << "You need to run this app with sudo." << std::endl;
        return -1;
    }
    #endif

    llfix::FileSystemUtilities::delete_directory_if_exists("messages_outgoing");
    llfix::FileSystemUtilities::delete_directory_if_exists("messages_incoming");
    llfix::FileSystemUtilities::delete_file_if_exists("sequence.store");

    llfix::ObjectCache<Order> order_cache;

    if (order_cache.create(2048) == false)
    {
        std::cout << "Could not create order_cache" << std::endl;
        return -1;
    }

    llfix::FixClientSettings client_settings;

    if(!client_settings.load_from_config_file("config.cfg", "EXAMPLE_CLIENT"))
    {
        std::cout << "Failed to load FIX client settings" << std::endl;
        return -1;
    }

    llfix::FixSessionSettings session_settings;

    if(!session_settings.load_from_config_file("config.cfg", "EXAMPLE_SESSION"))
    {
        std::cout << "Failed to load FIX session settings" << std::endl;
        return -1;
    }

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
        std::cout << "Connection to server failed. Please start the venue simulator before runnning this benchmark and check client's login settings." << std::endl;
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

    auto cpu_frequency = ProcessorUtilities::get_current_cpu_frequency_hertz();

    std::cout << "CPU frequency " << cpu_frequency << " hertz\n";

    #ifdef __linux__
    std::cout << "CPU isolation config : " << LinuxInfo::get_cpu_isolation_info() << "\n";
    #endif

    if(CPU_CORE_INDEX_TO_PIN+1 > llfix::ThreadUtilities::get_number_of_physical_cores())
    {
        std::cout << "Please change CPU_CORE_INDEX_TO_PIN and re build as this host doesn't have that core index." << std::endl;
        return -1;
    }

    llfix::ThreadUtilities::pin_calling_thread_to_cpu_core(CPU_CORE_INDEX_TO_PIN);
    std::cout << "Pinned to CPU core " << CPU_CORE_INDEX_TO_PIN << "\n";

    std::cout << "AVX2 available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx2() ? "true" : "false") << "\n";
    std::cout << "AVX512F available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512f() ? "true" : "false") << "\n";
    std::cout << "AVX512BW available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512bw() ? "true" : "false") << "\n";
    std::cout << "AVX512FBW available : " << (llfix::SIMDCapabilities::instance().supports_simd_avx512fbw() ? "true" : "false") << "\n";

    std::cout << "Antistall mock order count : " << ANTISTALL_MOCK_ORDER_COUNT << "\n";

    Stopwatch<StopwatchType::STOPWATCH_WITH_RDTSCP> stopwatch;
    Statistics<> stats;

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // NEW ORDER BENCMHARK INCLUDING NETWORKING
    const std::string symbol = "NOKIA.HE";

    for(std::size_t i =0; i<ITERATION_COUNT_NEW_ORDER+ANTISTALL_MOCK_ORDER_COUNT; i++)
    {
        stopwatch.start();

        auto new_order = order_cache.allocate();
        new_order->set_symbol(symbol);
        new_order->set_remaining_qty(10);
        new_order->set_price(10000);

        client.send_new_order<>(new_order);

        stopwatch.stop();

        if(i>=ANTISTALL_MOCK_ORDER_COUNT)
        {
            stats.add_sample(static_cast<double>(stopwatch.get_elapsed_nanoseconds(cpu_frequency)));
        }
    }

    stats.print("New order");

    //////////////////////////////////////////////////////////////////////////////////////////////////
    client.shutdown(false);

    #if _WIN32
    std::system("pause");
    #endif
    return 0;
}