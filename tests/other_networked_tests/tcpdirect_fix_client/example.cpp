///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES

//#define ENABLE_SOLARFLARE_SHARED_STACK    // Demonstrates shared TCPDirect stack usage

// Pass environment variable TCPDIRECT_ENABLE_HW_TX_TIMESTAMPS to try retrieving NIC HW TX timestamps

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>

#include "order.h"
#include "csv_file.h"

#include <llfix/engine.h>
#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>

#ifdef ENABLE_SOLARFLARE_SHARED_STACK
#include <llfix/core/solarflare_tcpdirect/stack.h>
#endif

#include "sample_client.h"

int main()
{
    const std::string config_file = "config.cfg";
    llfix::Engine::on_start(config_file);

    SampleClient client;

    if (client.create_task_queue() == false)
    {
        std::cout << "Failed to create task queue\n";
        return -1;
    }

    const std::string session_name = "EXAMPLE_SESSION";
    const std::string instance_name = "EXAMPLE_CLIENT";

    #ifndef ENABLE_SOLARFLARE_SHARED_STACK
    // DEFAULT : PRIVATE STACK PER CLIENT
    if (client.create(config_file, instance_name, config_file, session_name) == false)
    {
        std::cout << "FIX client creation failed. Check the logs.\n";
        return -1;
    }
    #else
    // SHARED STACK - SHOULD NOT BE ACCESSED FROM MULTIPLE THREADS AS TCPDIRECT STACK IS NOT THREAD SAFE
    llfix::FixClientSettings fix_client_settings;

    if(fix_client_settings.load_from_config_file(config_file, instance_name) == false)
    {
        std::cout << "Initialising FIX client config file failed\n";
    }

    std::unique_ptr<llfix::Stack> shared_stack(new llfix::Stack);

    llfix::StackProperties stack_properties;
    stack_properties.interface = fix_client_settings.nic_name;

    if(shared_stack->create(stack_properties) == false)
    {
        std::cout << "Creating shared stack failed\n";
        return -1;
    }

    fix_client_settings.stack = shared_stack.get();

    llfix::FixSessionSettings session_settings;

    if (session_settings.load_from_config_file(config_file, session_name) == false)
    {
        LLFIX_LOG_ERROR("Loading settings for session " + session_name + " failed : " + session_settings.config_load_error);
        return false;
    }

    if (client.create(instance_name, fix_client_settings, session_name, session_settings) == false)
    {
        std::cout << "FIX client creation failed. Check the logs.\n";
        return -1;
    }
    #endif

    llfix::Engine::get_management_server().register_client(&client);

    if(client.start() == false)
    {
        std::cout << "FIX client thread creation failed.\n";
        return -1;
    }

    std::string user_input;
    
    bool tx_hw_timestamps_enabled = false;
    const char* enable_hw_timestamps_env_variable = std::getenv("TCPDIRECT_ENABLE_HW_TX_TIMESTAMPS");

    if(enable_hw_timestamps_env_variable)
        tx_hw_timestamps_enabled = true;

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
        if(tx_hw_timestamps_enabled)
            llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 9 to retrieve NIC TX timestamps\n");
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
            Task* new_task = new Task;
            new_task->task_type = TaskType::CONNECT;
            client.push_task(new_task);
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
        
        if(tx_hw_timestamps_enabled)
            if (user_input[0] == '9')
            {
                llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Timestamp count :\n");
                std::string params;
                std::cin >> params;

                try
                {
                    int timestamp_count = std::stoi(params);

                    if(timestamp_count > 0 )
                    {
                        struct zf_pkt_report* reports = static_cast<struct zf_pkt_report*>(malloc(timestamp_count*sizeof(struct zf_pkt_report)));

                        if(reports)
                        {
                            auto ret_zft_get_tx_timestamps = client.get_tx_hw_timestamps(reports, &timestamp_count);

                            if(ret_zft_get_tx_timestamps == 0 )
                            {
                                for(int i =0;i<timestamp_count;i++)
                                {
                                    fprintf(stderr, "TIME SENT  %ld.%09ld bytes %u flags %x\n",
                                            reports[i].timestamp.tv_sec,
                                            reports[i].timestamp.tv_nsec,
                                            reports[i].bytes,
                                            reports[i].flags);
                                }
                                
                                if(timestamp_count == 0)
                                {
                                    llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "No reports found. Try to send orders.\n");
                                }
                            }
                            else
                            {
                                llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "zft_get_tx_timestamps failed , error code : " + std::to_string(ret_zft_get_tx_timestamps));
                            }

                            free(reports);
                        }
                        else
                        {
                            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Failed to allocate zf_pkt_report reports\n");
                        }
                    }
                    else
                    {
                        llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a positive timestamp count\n");
                    }
                }
                catch (...) {}
            }
    }

    llfix::Engine::stop_management_server();
    client.shutdown();
    llfix::Engine::shutdown();

    return 0;
}