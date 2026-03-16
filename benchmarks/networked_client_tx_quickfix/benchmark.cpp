///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define MEMORY_PERSISTENT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARAMS
#include <cstddef>

#if __linux__
#include <unistd.h> // geteuid for sudo check
#endif
static constexpr int            CPU_CORE_INDEX_TO_PIN = 10;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <llfix/common.h>
#include "../benchmark_utilities.h"


#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/os/thread_utilities.h>

#include <cstdint>
#include <memory>

#include "quickfix/SocketInitiator.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/FileStore.h"
#include "quickfix/MessageStore.h"

#include "client.h"

class NoLogFactory : public FIX::LogFactory
{
public:
    virtual ~NoLogFactory()
    {

    }

    virtual FIX::Log* create()
    {
        return nullptr;
    }

    virtual FIX::Log* create(const FIX::SessionID&)
    {
        return nullptr;
    }

    virtual void destroy(FIX::Log*)
    {
    }
};

int main(int argc, char* argv[])
{
    #if __linux__
    if (geteuid() != 0)
    {
        std::cout << "You need to run this app with sudo." << std::endl;
        return -1;
    }
    #endif

    llfix::FileSystemUtilities::delete_directory_if_exists("stores");

    #ifdef MEMORY_PERSISTENT
    std::cout << "QUICKFIX BM MODE : MEMORY PERSISTENT\n";
    const std::string config_file = "client_memory_persistent.cfg";
    #else
    std::cout << "QUICKFIX BM MODE : FILE PERSISTENT\n";
    const std::string config_file = "client_file_persistent.cfg";
    #endif

    FIX::SessionSettings settings(config_file);
    NoLogFactory log_factory;

    Client client;
    std::unique_ptr<FIX::Initiator> initiator;

    #ifdef MEMORY_PERSISTENT
    FIX::MemoryStoreFactory storeFactory;
    #else
    FIX::FileStoreFactory storeFactory(settings);
    #endif

    initiator = std::unique_ptr<FIX::Initiator>(new FIX::SocketInitiator(client, storeFactory, settings, log_factory));

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
    // NEW ORDER BENCMHARK INCLUDING NETWORKING
    client.set_start_benchmark(true);
    initiator->start();
    client.run();
    initiator->stop();

    client.get_stats().print("Quickfix New order");
    //////////////////////////////////////////////////////////////////////////////////////////////////
    #if _WIN32
    std::system("pause");
    #endif
    return 0;
}