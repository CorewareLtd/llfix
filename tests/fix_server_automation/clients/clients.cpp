#include "quickfix/SocketInitiator.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/FileStore.h"
#include "quickfix/Log.h"

#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <vector>

#include "client_application50.h"
#include "client_application44.h"
#include "client_application43.h"
#include "client_application42.h"

template <typename ClientApplicationType>
void run_fix_client(const std::string& config_file)
{
    try
    {
        FIX::SessionSettings settings(config_file);

        ClientApplicationType application;

        FIX::FileStoreFactory storeFactory(settings);
        FIX::ScreenLogFactory logFactory(settings);
        std::unique_ptr<FIX::Initiator> initiator;

        initiator = std::unique_ptr<FIX::Initiator>(new FIX::SocketInitiator(application, storeFactory, settings, logFactory));

        initiator->start();
        application.run();
        initiator->stop();

    }
    catch (std::exception& e)
    {
        std::cout << e.what();
    }
}

int main()
{
    std::vector<std::thread*> threads;
    ///////////////////////////////////////////////////////////////////////////////

    // CLIENT1 , FIX5.0
    auto client1_thread = new std::thread(run_fix_client<ClientApplication50>, "configs/client1.cfg");
    threads.push_back(client1_thread);

    // CLIENT2 , FIX4.4
    auto client2_thread = new std::thread(run_fix_client<ClientApplication44>, "configs/client2.cfg");
    threads.push_back(client2_thread);

    // CLIENT3 , FIX4.3
    auto client3_thread = new std::thread(run_fix_client<ClientApplication43>, "configs/client3.cfg");
    threads.push_back(client3_thread);

    // CLIENT4 , FIX4.2
    auto client4_thread = new std::thread(run_fix_client<ClientApplication42>, "configs/client4.cfg");
    threads.push_back(client4_thread);

    ///////////////////////////////////////////////////////////////////////////////
    for (const auto& thr : threads)
    {
        thr->join();
        delete thr;
    }

    return 0;
}