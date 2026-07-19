///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define MEMORY_PERSISTENT
//#define ENABLE_TCPDIRECT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARAMS
#include <cstddef>
#include <string>

#if __linux__
#include <unistd.h> // geteuid for sudo check
#endif

static constexpr std::size_t    ITERATION_COUNT_NEW_ORDER = 1'000'000;
static constexpr std::size_t    ANTISTALL_MOCK_ORDER_COUNT = 1000;
static constexpr int            CPU_CORE_INDEX_TO_PIN = 10;

static const std::string TARGET_IP = "192.168.10.100"; // Change accordingly
static constexpr int TARGET_PORT = 5001;
static constexpr int HEARTBEAT_INTERVAL_SECS = 60;
static const std::string SENDER_COMPID = "CLIENT1";
static const std::string TARGET_COMPID = "EXECUTOR";
static const std::string SENDER_SUBID = "SNDR_SUB";
static const std::string TARGET_SUBID = "SRVR_SUB";
static const std::string NIC_NAME = "ens3f0"; // Change accordingly
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "../benchmark_utilities.h"

#include <llfix/core/os/thread_utilities.h>
#include <llfix/core/cpu/simd_capabilities.h>

#include <OnixS/FIXEngine.h>
#include <OnixS/FIXEngine/FIX/ProtocolVersion.h>
#include <OnixS/FIXEngine/FIX/FIX50SP2.h>
using namespace OnixS::FIX;

const OnixS::Threading::CpuIndex OnixSThreadAffinity = CPU_CORE_INDEX_TO_PIN;

#include "listener.h"

#include <cstdint>
#include <cstdio>

int main()
{
    #if __linux__
    if (geteuid() != 0)
    {
        std::cout << "You need to run this app with sudo." << std::endl;
        return -1;
    }
    #endif

    #ifdef MEMORY_PERSISTENT
    fprintf(stdout, "ONIXS MODE : MEMORY PERSISTENT\n");
    #else
    fprintf(stdout, "ONIXS MODE : FILE PERSISTENT\n");
    #endif

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
    const std::string LicenseStore = "./license|../license|../../license";

    EngineSettings settings;

    // Settings below are taken from OnixS SDK's latency benchmark example
    settings.dictionaryFile("LowLatencyDictionary.xml")
            .licenseStore(LicenseStore)
            .resendingQueueSize(0)
            .sendLogoutOnInvalidLogon(true)
            .createEngineLogFile(false)
            .logInboundMessages(false)
            .logOutboundMessages(false)
            .validateFieldValues(false)
            .validateRequiredFields(false)
            .validateUnknownFields(false)
            .validateUnknownMessages(false)
            .receiveSpinningTimeout(10000)
            .logBeforeSending(false);

    #ifndef ENABLE_TCPDIRECT
    settings.useSpinLock(true);
    #endif

    Engine::init(settings);

    Listener listener;


    #ifdef ENABLE_TCPDIRECT
    TCPDirect::Attributes attr;
    attr.networkInterface(NIC_NAME);
    TCPDirect::Stack tcpdirect_stack(attr);
    #endif

    const Dictionary dictionary("LowLatencyDictionaryId");

    #ifdef MEMORY_PERSISTENT
    #ifdef ENABLE_TCPDIRECT
    Session session(&tcpdirect_stack, SENDER_COMPID, TARGET_COMPID, dictionary, &listener, OnixS::FIX::SessionStorageType::MemoryBased);
    #else
    Session session(SENDER_COMPID, TARGET_COMPID, dictionary, &listener, OnixS::FIX::SessionStorageType::MemoryBased);
    #endif
    #else
    #ifdef ENABLE_TCPDIRECT
    Session session(&tcpdirect_stack, SENDER_COMPID, TARGET_COMPID, dictionary, &listener);
    #else
    Session session(SENDER_COMPID, TARGET_COMPID, dictionary, &listener);
    #endif
    #endif

    session.senderSubId(SENDER_SUBID).targetSubId(TARGET_SUBID);
    session.sendingTimeFormat(OnixS::FIX::TimestampFormat::YYYYMMDDHHMMSSUsec);
    session.sendingThreadAffinity(OnixSThreadAffinity).receivingThreadAffinity(OnixSThreadAffinity);

    Message customLogon(OnixS::FIX::FIX50_SP2::Values::MsgType::Logon, dictionary);
    customLogon.set(OnixS::FIX::FIX50_SP2::Tags::DefaultApplVerID, "7");

    #ifdef ENABLE_TCPDIRECT
    const OnixS::Threading::SharedFuture<void> connected = session.logonAsInitiatorAsync(TARGET_IP, TARGET_PORT, HEARTBEAT_INTERVAL_SECS, ONIXS_FIXENGINE_NULLPTR, true);

    while(!connected.isReady())
        tcpdirect_stack.dispatchEvents();

    if(connected.hasException())
            connected.get();
    #else
    session.logonAsInitiator(TARGET_IP, TARGET_PORT, HEARTBEAT_INTERVAL_SECS, &customLogon);
    #endif

    const std::string symbol = "NOKIA.HE";

    for(std::size_t i =0; i<ITERATION_COUNT_NEW_ORDER+ANTISTALL_MOCK_ORDER_COUNT; i++)
    {
        stopwatch.start();

        // Fixed order id/ClOrdID is not realistic but that is for benchmark fairness to avoid std::string construction
        Message order(OnixS::FIX::FIX50_SP2::Values::MsgType::NewOrderSingle, dictionary);
        order.set(OnixS::FIX::FIX50_SP2::Tags::ClOrdID, "1")
            .set(OnixS::FIX::FIX50_SP2::Tags::Symbol, symbol)
            .set(OnixS::FIX::FIX50_SP2::Tags::Side, '1')
            .set(OnixS::FIX::FIX50_SP2::Tags::OrderQty, 10)
            .set(OnixS::FIX::FIX50_SP2::Tags::Price, 10000)
            .set(OnixS::FIX::FIX50_SP2::Tags::OrdType, '2')
            .set(OnixS::FIX::FIX50_SP2::Tags::TimeInForce, '0')
            .set(OnixS::FIX::FIX50_SP2::Tags::TransactTime, OnixS::FIX::Timestamp::utc(), OnixS::FIX::TimestampFormat::YYYYMMDDHHMMSSUsec);

        OnixS::FIX::Group parties = order.setGroup(OnixS::FIX::FIX50_SP2::Tags::NoPartyIDs, 2);
        parties[0].set(OnixS::FIX::FIX50_SP2::Tags::PartyID, "PARTY1")
            .set(OnixS::FIX::FIX50_SP2::Tags::PartyIDSource, 'D')
            .set(OnixS::FIX::FIX50_SP2::Tags::PartyRole, 1);
        parties[1].set(OnixS::FIX::FIX50_SP2::Tags::PartyID, "PARTY2")
            .set(OnixS::FIX::FIX50_SP2::Tags::PartyIDSource, 'D')
            .set(OnixS::FIX::FIX50_SP2::Tags::PartyRole, 3);

        session.send(&order);

        stopwatch.stop();

        if(i>=ANTISTALL_MOCK_ORDER_COUNT)
        {
            stats.add_sample(static_cast<double>(stopwatch.get_elapsed_nanoseconds(cpu_frequency)));
        }
        
        #ifdef ENABLE_TCPDIRECT
        tcpdirect_stack.dispatchEvents();
        #endif
    }
    
    #ifdef ENABLE_TCPDIRECT
    const OnixS::Threading::SharedFuture<void> disconnected = session.logoutAsync("");

    while(!disconnected.isReady())
        tcpdirect_stack.dispatchEvents();

    if(disconnected.hasException())
        disconnected.get();

    session.shutdown();

    while(tcpdirect_stack.isQuiescent())
        tcpdirect_stack.dispatchEvents();
    #endif

    stats.print("OnixS New order");
    //////////////////////////////////////////////////////////////////////////////////////////////////

    #if _WIN32
    std::system("pause");
    #endif
    return 0;
}
