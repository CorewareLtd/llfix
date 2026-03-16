#include "test_server.h"

#include "quickfix/FileStore.h"
#include "quickfix/SocketAcceptor.h"

#include "quickfix/ThreadedSocketAcceptor.h"

#include <llfix/core/utilities/configuration.h>

#include <iostream>
#include <memory>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: test_server <config_file>\n";
        return -1;
    }

    std::string file = argv[1];
    std::cout << "Quickfix config file : " << file << "\n";

    llfix::Configuration automation_config;
    std::string autoation_config_load_error;

    if (automation_config.load_from_file("../automation/config.cfg", autoation_config_load_error) == false)
    {
        std::cout << "Failed to load automation config\n";
        return -1;
    }

    try
    {
        TestServer server(automation_config.get_string_value("server_username"), automation_config.get_string_value("server_password"), automation_config.get_string_value("fill_instrument"), automation_config.get_string_value("reject_instrument"));

        FIX::SessionSettings settings(file);
        FIX::FileStoreFactory store_factory(settings);
        FIX::ScreenLogFactory log_factory(settings);

        std::unique_ptr<FIX::Acceptor> acceptor;

        #ifdef THREAD_PER_CLIENT
        acceptor = std::unique_ptr<FIX::Acceptor>( new FIX::ThreadedSocketAcceptor(server, store_factory, settings, log_factory));
        #else
        acceptor = std::unique_ptr<FIX::Acceptor>( new FIX::SocketAcceptor(server, store_factory, settings, log_factory));
        #endif

        acceptor->start();

        std::cout << "Type Ctrl-C to quit" << std::endl;

        while (true)
        {
            FIX::process_sleep(1);
        }

        acceptor->stop();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 1;
    }

    return 0;
}