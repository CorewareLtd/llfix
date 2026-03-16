///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define MEMORY_PERSISTENT

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARAMS
#include <cstddef>

#if __linux__
#include <unistd.h> // geteuid for sudo check
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <llfix/common.h>
#include "../benchmark_utilities.h"

#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/os/thread_utilities.h>

#include <cstdint>
#include <memory>

#include "client.h"

int main(int argc, char* argv[])
{
    #if __linux__
    if (geteuid() != 0)
    {
        std::cout << "You need to run this app with sudo." << std::endl;
        return -1;
    }
    #endif

    Client client;

    auto cpu_frequency = ProcessorUtilities::get_current_cpu_frequency_hertz();
    client.set_cpu_frequency(cpu_frequency);

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

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // NEW ORDER BENCHMARK INCLUDING NETWORKING
    client.set_start_benchmark(true);
    client.start();
    client.run();
    client.stop();

    client.get_stats().print("Fix8 New order");
    //////////////////////////////////////////////////////////////////////////////////////////////////
    #if _WIN32
    std::system("pause");
    #endif
    return 0;
}