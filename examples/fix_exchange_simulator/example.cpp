#include <llfix/engine.h>
#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>

#include "exchange_simulator.h"

#include <string>
#include <sstream>
#include <iostream>

int main()
{
    std::string config_file = "config.cfg";

    llfix::Engine::on_start(config_file);
    ExchangeSimulator simulator;

    if (simulator.create("EXAMPLE_SERVER", config_file) == false)
    {
        std::cout << "Failed to create the FIX server. Check the logs\n";
        return -1;
    }

    if (simulator.add_sessions_from(config_file) == false)
    {
        std::cout << "Failed to load sessions from " << config_file << ". Check the logs\n";
        return -1;
    }

    simulator.specify_repeating_group("D", 453, 448, 447, 452); // This is not needed when you use dictionaries

    simulator.init_stats_per_session();

    llfix::Engine::get_management_server().register_server(&simulator);

    simulator.start();

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press i to see the all session info\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press s to see the all session stats\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press d to see the simulator mode\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press x to change the simulator mode\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            break;
        }
        else if (user_input[0] == 'i')
        {
            std::stringstream output;
            output << '\n' << simulator.get_all_sessions_display_text() << '\n';
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }
        else if (user_input[0] == 's')
        {
            std::stringstream output;
            output << '\n' << simulator.get_all_sessions_stats_display_text() << '\n';
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }
        else if (user_input[0] == 'd')
        {
            std::string simulator_mode;

            switch (simulator.mode())
            {
                case SimulatorMode::NO_FILLS: simulator_mode = "NO_FILLS"; break;
                case SimulatorMode::FILL_NEW_ORDER_AT_ONCE: simulator_mode = "FILL_NEW_ORDER_AT_ONCE"; break;
                case SimulatorMode::FILL_NEW_ORDER_ONE_BY_ONE: simulator_mode = "FILL_NEW_ORDER_ONE_BY_ONE"; break;
                case SimulatorMode::FILL_REPLACE_ORDER_AT_ONCE: simulator_mode = "FILL_REPLACE_ORDER_AT_ONCE"; break;
                case SimulatorMode::FILL_REPLACE_ORDER_ONE_BY_ONE: simulator_mode = "FILL_REPLACE_ORDER_ONE_BY_ONE"; break;
                case SimulatorMode::REJECT_ALL_REQUESTS: simulator_mode = "REJECT_ALL_REQUESTS"; break;
            }

            std::stringstream output;
            output << "Mode = " << simulator_mode << "\n\n";
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }

        else if (user_input[0] == 'x')
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Choose a simulator mode by entering a number: 0 (NO_FILLS) 1(FILL_NEW_ORDER_ONE_BY_ONE) 2(FILL_NEW_ORDER_AT_ONCE) 3(FILL_REPLACE_ORDER_ONE_BY_ONE) 4(FILL_REPLACE_ORDER_AT_ONCE) 5(REJECT_ALL_REQUESTS)\n");

            std::string choice;
            std::cin >> choice;

            try
            {
                auto int_choice = std::stoi(choice);

                if(int_choice >= 0 && int_choice <= 5 )
                {
                    auto actual_choice = static_cast<SimulatorMode>(int_choice);
                    simulator.set_mode(actual_choice);
                }
            }
            catch (...) {}
        }
    }

    llfix::Engine::stop_management_server();
    simulator.shutdown();
    llfix::Engine::shutdown();

    return 0;
}