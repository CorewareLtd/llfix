#include <llfix/engine.h>
#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>

#include <llfix/fix_server.h>
#include <llfix/core/utilities/tcp_reactor.h>

#include <llfix/fix_client.h>
#include <llfix/core/utilities/tcp_connector.h>

#include "order_router.h"

#include <string>
#include <sstream>
#include <iostream>

int main()
{
    llfix::Engine::on_start("configs/engine.cfg");

    OrderRouter< llfix::FixServer<llfix::TcpReactor<>>, llfix::FixClient<llfix::TCPConnector>> router;

    if (!router.initialise("configs/routing.cfg", "configs/inbound.cfg"))
    {
        return -1;
    }

    router.specify_repeating_group("D", 453, 448, 447, 452); // This is not needed when you use dictionaries

    // Inbound sessions
    router.add_inbound_session("configs/inbound.cfg", "SESSION_INBOUND1");
    router.add_inbound_session("configs/inbound.cfg", "SESSION_INBOUND2");

    // Outbound sessions
    router.add_outbound_session("configs/outbound.cfg", "CLIENT_OUTBOUND1", "configs/outbound.cfg", "SESSION_OUTBOUND1");
    router.add_outbound_session("configs/outbound.cfg", "CLIENT_OUTBOUND2", "configs/outbound.cfg", "SESSION_OUTBOUND2");

    // Management server
    llfix::Engine::get_management_server().register_server(&router);

    router.start();

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            break;
        }
    }

    llfix::Engine::stop_management_server();
    router.shutdown();
    llfix::Engine::shutdown();

    return 0;
}