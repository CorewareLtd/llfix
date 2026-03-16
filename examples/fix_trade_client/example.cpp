#include <llfix/engine.h>
#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>

#include <string>
#include <sstream>
#include <iostream>

#include "order.h"
#include "csv_file.h"

#include "sample_client.h"

int main()
{
    llfix::Engine::on_start("config.cfg");

    SampleClient client;

    if (client.create_task_queue() == false)
    {
        std::cout << "Failed to create task queue\n";
        return -1;
    }

    if (client.create("config.cfg", "EXAMPLE_CLIENT", "config.cfg", "EXAMPLE_SESSION") == false)
    {
        std::cout << "Fix client creation failed. Check the logs.\n";
        return -1;
    }

    llfix::Engine::get_management_server().register_client(&client);

    if(client.start() == false)
    {
        std::cout << "Fix client thread creation failed.\n";
        return -1;
    }

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 1 to see session details\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 2 to connect\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 3 to see the order book\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 4 to send new buy order\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 5 to send new sell order\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 6 to send replace order\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 7 to send cancel order\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 8 to send orders from a CSV file\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            break;
        }
        else if (user_input[0] == '1') // SEE THE SESSION DETAILS
        {
            std::stringstream output;
            output << '\n' << client.get_session()->get_display_text() << '\n';
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }
        else if (user_input[0] == '2') // CONNECT
        {
            try
            {
                Task* new_task = new Task;
                new_task->task_type = TaskType::CONNECT;
                client.push_task(new_task);
            }
            catch (...) {}
        }
        else if (user_input[0] == '3') // SEE THE ORDER BOOK
        {
            auto info = client.get_orders_as_string();

            if (info.length() == 0)
            {
                info = "No orders in the order book.\n";
            }

            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, info);
        }
        else if (user_input[0] == '4' || user_input[0] == '5') // SEND NEW BUY/SELL ORDER
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Symbol, price, quantity and TIF as comma-separated (1:DAY, 4:FOK, 5:IOC) :\n");
            std::string params;
            std::cin >> params;

            try
            {
                auto tokens = llfix::StringUtilities::split(params, ',');

                if (tokens.size() >= 4)
                {
                    auto symbol = (tokens[0]);
                    auto str_price = tokens[1];
                    auto quantity = std::stoi(tokens[2]);
                    TimeInForce tif = static_cast<TimeInForce>(std::stoi(tokens[3]));

                    auto new_order = new Order;

                    new_order->set_remaining_qty(quantity);
                    new_order->set_price(std::stoi(str_price));

                    new_order->set_symbol(symbol);
                    new_order->set_tif(tif);

                    Task* new_task = new Task;
                    new_task->order = new_order;

                    if (user_input[0] == '4')
                    {
                        new_task->task_type = TaskType::NEW_ORDER_BUY;
                    }
                    else
                    {
                        new_task->task_type = TaskType::NEW_ORDER_SELL;
                    }

                    client.push_task(new_task);
                }
            }
            catch (...) {}
        }
        else if (user_input[0] == '6') // REPLACE ORDER
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Orig order id, price and quantity as comma-separated :\n");
            std::string params;
            std::cin >> params;

            try
            {
                auto tokens = llfix::StringUtilities::split(params, ',');

                if (tokens.size() >= 3)
                {
                    auto orig_order_id = std::stoi(tokens[0]);
                    auto str_price = tokens[1];
                    auto quantity = std::stoi(tokens[2]);

                    Order* orig_order = client.get_order(orig_order_id);

                    if (orig_order != nullptr)
                    {
                        auto replace_order = new Order;

                        replace_order->set_symbol(orig_order->get_symbol());
                        replace_order->set_numeric_order_id(orig_order_id);
                        replace_order->set_remaining_qty(quantity);
                        replace_order->set_price(std::stoi(str_price));

                        replace_order->set_type(orig_order->get_type());
                        replace_order->set_side(orig_order->get_side());

                        Task* new_task = new Task;
                        new_task->order = replace_order;
                        new_task->orig_order = orig_order;
                        new_task->task_type = TaskType::REPLACE_ORDER;

                        client.push_task(new_task);
                    }
                    else
                    {
                        llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Could not find specified orig order\n");
                    }
                }
            }
            catch (...) {}
        }
        else if (user_input[0] == '7') // CANCEL ORDER
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Orig order id :\n");
            std::string params;
            std::cin >> params;

            try
            {
                auto orig_order_id = std::stoi(params);

                Order* orig_order = client.get_order(orig_order_id);

                Task* new_task = new Task;
                new_task->orig_order = orig_order;
                new_task->task_type = TaskType::CANCEL_ORDER;
                client.push_task(new_task);
            }
            catch (...) {}
        }
        else if (user_input[0] == '8') // ORDERS FROM A CSV FILE
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Enter CSV order file path :\n");
            std::string orders_file;
            std::cin >> orders_file;
            std::size_t order_count = 0;

            try
            {
                CSVFile csv_orders_file;

                if(csv_orders_file.load_from(orders_file))
                {
                    for(auto& row : csv_orders_file)
                    {
                        order_count++;

                        auto new_order = new Order;

                        new_order->set_type(OrderType::LIMIT);
                        new_order->set_remaining_qty(std::stoi(row["QUANTITY"]));
                        new_order->set_price(std::stoi(row["PRICE"]));
                        new_order->set_symbol(row["SYMBOL"]);

                        Task* new_task = new Task;
                        new_task->order = new_order;

                        if (row["SIDE"][0] == 'S')
                        {
                            new_task->task_type = TaskType::NEW_ORDER_SELL;
                        }
                        else
                        {
                            new_task->task_type = TaskType::NEW_ORDER_BUY;
                        }

                        client.push_task(new_task);
                    }
                }
                else
                {
                    llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "An error occured when reading the orders file\n");
                }
            }
            catch (...)
            {
                llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "An error occured when reading the orders file\n");
            }

            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, std::to_string(order_count) + " orders added to the task queue\n");
        }
    }

    llfix::Engine::stop_management_server();
    client.shutdown();
    llfix::Engine::shutdown();

    return 0;
}