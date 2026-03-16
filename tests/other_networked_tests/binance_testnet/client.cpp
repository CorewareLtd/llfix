///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define LLFIX_ENABLE_OPENSSL
#define LLFIX_ENABLE_DICTIONARY
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <llfix/engine.h>

#include "binance_client.h"

#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/utilities/spsc_bounded_queue.h>

#include <atomic>
#include <cstddef>
#include <string>
#include <sstream>
#include <thread>
#include <memory>
#include <new>
#include <iostream>

enum class TaskType
{
    NONE,
    SEND_BULK_ORDER,
    SEND_QUERY_LIMITS
};

struct Task
{
    TaskType task_type = TaskType::NONE;
    int order_count = 0;
    bool is_buy = true;
    std::string symbol = "";
    double qty = 0;
    std::size_t qty_decimal_points = 0;
    int price = 0;
};

llfix::SPSCBoundedQueue<Task*> task_queue;
Task* get_bulk_order_inputs_from_user();

int main(int argc, char* argv[])
{
    std::string config_file = "config.cfg";

    if (argc > 1)
    {
        config_file = argv[1];
    }

    llfix::Engine::on_start(config_file);

    if (task_queue.create(1024) == false)
    {
        std::cout << "Failed to create task queue.\n";
        return -1;
    }

    BinanceClient client;

    if (client.create(config_file, "EXAMPLE_CLIENT", config_file, "EXAMPLE_SESSION") == false)
    {
        std::cout << "Fix client creation failed. Check the logs.\n";
        return -1;
    }

    bool connection_success = client.connect();

    if (connection_success == false)
    {
        std::cout << "Connection to server failed. Please start the server before runnning this process and check the logs." << std::endl;
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
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::atomic<bool> is_exiting = false;

    auto client_thread_function = [&]()
        {
            while (true)
            {
                if (is_exiting.load() == true)
                    break;

                auto session_state = client.get_session()->get_state();

                if (session_state == llfix::SessionState::LOGGED_ON)
                {
                    client.process();

                    // POP TASK
                    Task* current_task = nullptr;
                    bool got_new_task = task_queue.try_pop(&current_task);

                    // EXECUTE THE POPPED TASK
                    if (got_new_task)
                    {
                        if (current_task->task_type == TaskType::SEND_BULK_ORDER)
                        {
                            for (std::size_t j = 0; j < (std::size_t)current_task->order_count; j++)
                            {
                                if (is_exiting.load() == true)
                                {
                                    return;
                                }

                                client.process();

                                if (current_task->is_buy)
                                    client.send_new_order<true>(current_task->symbol, current_task->qty, current_task->qty_decimal_points, current_task->price);
                                else
                                    client.send_new_order<false>(current_task->symbol, current_task->qty, current_task->qty_decimal_points, current_task->price);
                            }
                        }
                        else if (current_task->task_type == TaskType::SEND_QUERY_LIMITS)
                        {
                            client.send_query_limits();
                        }

                        delete current_task;
                        
                        if (is_exiting.load() == true)
                        {
                            return;
                        }
                    }
                }
            }
        };

    std::unique_ptr<std::thread> client_thread(new std::thread(client_thread_function));

    std::string user_input;

    while (true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 1 to see session details\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 2 to send buy orders\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 3 to send sell orders\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 4 to send query limit\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            client.shutdown(true);
            is_exiting.store(true);
            break;
        }
        else if (user_input[0] == '1')
        {
            std::stringstream output;
            output << client.get_session()->get_display_text() << '\n';
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }
        else if (user_input[0] == '2')
        {
            Task* new_task = get_bulk_order_inputs_from_user();

            if(new_task)
            { 
                new_task->is_buy = true;
                task_queue.push(new_task);
                llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Added to the task queue\n");
            }
        }
        else if (user_input[0] == '3')
        { 
            Task* new_task = get_bulk_order_inputs_from_user();

            if (new_task)
            {
                new_task->is_buy = false;
                task_queue.push(new_task);
                llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Added to the task queue\n");
            }
        }
        else if (user_input[0] == '4')
        {
            Task* new_task = new (std::nothrow) Task;

            if (new_task)
            {
                new_task->task_type = TaskType::SEND_QUERY_LIMITS;
                task_queue.push(new_task);
                llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Added to the task queue\n");
            }
        }
    }

    client_thread->join();

    return 0;
}

Task* get_bulk_order_inputs_from_user()
{
    Task* ret = nullptr;

    try
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Please enter order count and press enter :");
        int order_count{ 0 };
        std::string string_order_count;
        std::cin >> string_order_count;

        order_count = std::stoi(string_order_count);

        if (order_count <= 0)
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a positive order count\n");
            return ret;
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Please enter symbol and press enter (Ex: BTCUSDT) :");
        std::string symbol;
        std::cin >> symbol;

        if (symbol.empty())
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a valid symbol\n");
            return ret;
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Please enter quantity and press enter (Ex: 0.001) :" );
        double qty = 0;

        std::string string_qty;
        std::cin >> string_qty;

        qty = std::stod(string_qty);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Please enter quantity decimal points and press enter (Ex: 3) :");
        std::size_t qty_decimal_points = 0;

        std::string string_qty_decimal_points;
        std::cin >> string_qty_decimal_points;

        qty_decimal_points = static_cast<std::size_t>(std::stoul(string_qty_decimal_points));

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Please enter price and press enter (Ex: 69000):");
        int price = 0;

        std::string string_price;
        std::cin >> string_price;

        price = std::stoi(string_price);

        if (price <= 0)
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a positive price\n");
            return ret;
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        ret = new Task;
        ret->order_count = order_count;
        ret->symbol = symbol;
        
        ret->qty = qty;
        ret->qty_decimal_points = qty_decimal_points;
        ret->price = price;

        ret->task_type = TaskType::SEND_BULK_ORDER;
    }
    catch (...)
    {
    }

    return ret;
}