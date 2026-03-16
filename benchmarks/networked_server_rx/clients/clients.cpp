#include <atomic>
#include <string>
#include <sstream>
#include <iostream>
#include <thread>
#include <vector>
#include <new>

#include <llfix/engine.h>

#include "order.h"
#include "sample_client.h"

#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/utilities/spsc_bounded_queue.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/utilities/configuration.h>

enum class TaskType
{
    NONE,
    SEND_BULK_ORDER,
    DISCONNECT
};

struct Task
{
    TaskType task_type = TaskType::NONE;
    int order_count = 0;
};

llfix::SPSCBoundedQueue<Task*>* task_queues = nullptr;

int main(int argc, char* argv[])
{
    std::string config_file = "config_local.cfg";

    if(argc > 1 )
    {
        config_file = argv[1];
    }

    llfix::Engine::on_start(config_file);

    std::atomic<bool> is_exiting = false;

    //////////////////////////////////////////////////////////////////
    std::string config_load_error;
    llfix::Configuration config;

    if (config.load_from_file(config_file, config_load_error) == false)
    {
        std::cout << "Failed to load config file " << config_file << "\n";
        return -1;
    }

    auto client_count = config.get_int_value("client_count", 4);
    auto primary_address = config.get_string_value("primary_address", "127.0.0.1");
    auto primary_port = config.get_int_value("primary_port", 5001);
    auto nic_address = config.get_string_value("nic_address", "");
    auto nic_name = config.get_string_value("nic_name", "");
    auto serialised_file_max_size = config.get_int_value("serialised_file_max_size", 67108864);
    //////////////////////////////////////////////////////////////////

    SampleClient* clients = nullptr;
    clients = new (std::nothrow) SampleClient[client_count];

    if (clients == nullptr)
    {
        std::cout << "Failed to create clients\n";
        return -1;
    }

    task_queues = new(std::nothrow) llfix::SPSCBoundedQueue<Task*>[client_count];

    if (task_queues == nullptr)
    {
        std::cout << "Failed to create task queues\n";
        return -1;
    }

    for (int i = 0; i < client_count; i++)
    {
        llfix::FixClientSettings fix_client_settings;
        fix_client_settings.primary_address = primary_address;
        fix_client_settings.primary_port = primary_port;
        fix_client_settings.nic_address = nic_address;
        fix_client_settings.nic_name = nic_name;

        #ifdef LLFIX_ENABLE_OPENSSL
        fix_client_settings.use_ssl = true;
        fix_client_settings.ssl_version = "TLS12";
        fix_client_settings.ssl_certificate_pem_file = "./client.crt";
        fix_client_settings.ssl_private_key_pem_file = "./client.key";
        fix_client_settings.ssl_ca_pem_file = "./ca.crt";
        fix_client_settings.ssl_verify_peer = true;
        #endif

        llfix::FixSessionSettings fix_session_settings;
        fix_session_settings.begin_string = "FIXT.1.1";
        fix_session_settings.default_app_ver_id = "7";

        fix_session_settings.sender_comp_id = "CLIENT" + std::to_string(i+1);
        fix_session_settings.target_comp_id = "EXECUTOR";

        fix_session_settings.heartbeat_interval_in_nanoseconds = 100'000'000'000;

        fix_session_settings.validate_repeating_groups = true;

        fix_session_settings.throttle_window_in_milliseconds = 1;
        fix_session_settings.throttle_limit = 0;

        fix_session_settings.max_serialised_file_size = serialised_file_max_size;
        fix_session_settings.sequence_store_file_path = "the_clients/client" + std::to_string(i+1) + "/sequence.store";                 // the_clients/client1/sequence.store
        fix_session_settings.incoming_message_serialisation_path = "the_clients/client" + std::to_string(i + 1) + "/messages_incoming"; // the_clients/client1/messages_incoming
        fix_session_settings.outgoing_message_serialisation_path = "the_clients/client" + std::to_string(i + 1) + "/messages_outgoing"; // the_clients/client1/messages_outgoing

        fix_session_settings.initialise_derived_settings();

        std::string client_name = "CLIENT" + std::to_string(i + 1);
        std::string session_name = "SESSION" + std::to_string(i + 1);

        if(clients[i].create(client_name, fix_client_settings, session_name, fix_session_settings) == false)
        {
            std::cout << "Fix client creation failed\n";
            return -1;
        }

        llfix::Engine::get_management_server().register_client(&(clients[i]));
    }

    for (int i = 0; i < client_count; i++)
    {
        if (task_queues[i].create(10240) == false)
        {
            std::cout << "Task queue creation failed" << std::endl;
            return -1;
        }
    }

    auto clients_thread_function = [&](int index)
        {
            clients[index].initialise_thread();

            while (true)
            {
                if (is_exiting.load() == true)
                {
                    return;
                }

                auto state = clients[index].get_session()->get_state();

                if (state != llfix::SessionState::DISCONNECTED && state != llfix::SessionState::LOGGED_OUT)
                {
                    clients[index].process();
                }
                else if (state == llfix::SessionState::DISCONNECTED || state == llfix::SessionState::LOGGED_OUT)
                {
                    bool connection_success = clients[index].connect();

                    if (connection_success == true)
                    {
                        std::cout << "Connection to server success\n";
                    }
                }

                if (state == llfix::SessionState::LOGGED_ON)
                {
                    // POP TASK
                    Task* current_task = nullptr;
                    bool got_new_task = task_queues[index].try_pop(&current_task);

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

                                clients[index].process();

                                if (clients[index].get_session()->get_state() != llfix::SessionState::LOGGED_ON)
                                {
                                    std::cout << "Quitting the task as the session is not logged on any longer. No of sent orders : " << j << "\n";
                                    break;
                                }

                                //////////////////////////////////////////////
                                auto new_order = new Order;
                                new_order->set_type(OrderType::LIMIT);
                                new_order->set_remaining_qty(1);
                                new_order->set_price(5);
                                new_order->set_symbol("BMWG.DE");
                                clients[index].send_new_order<OrderType::LIMIT, OrderSide::BUY, TimeInForce::GFD>(new_order);
                            }
                        }
                        else if (current_task->task_type == TaskType::DISCONNECT)
                        {
                            clients[index].shutdown(true);
                        }

                        if (is_exiting.load() == true)
                        {
                            return;
                        }
                    }
                }

                if (is_exiting.load() == true)
                {
                    return;
                }
            }
        };

    std::vector<std::thread*> threads;

    for (int i = 0; i < client_count; i++)
    {
        auto client_thread = new std::thread(clients_thread_function, i);
        threads.push_back(client_thread);
    }

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 1 to see session details\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 2 to save order books to text file\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 3 to send orders from all clients\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 4 for reconnections test\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'q') // QUIT
        {
            is_exiting.store(true);
            break;
        }
        else if (user_input[0] == '1') // SEE THE SESSION DETAILS
        {
            std::stringstream output;

            for (int i = 0; i < client_count; i++)
            {
                output << "CLIENT" << std::to_string(i + 1) << "\n";
                output << clients[i].get_session()->get_display_text() << '\n';
            }

            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }
        else if (user_input[0] == '2') // SAVE ORDER BOOKS INTO A TEXT FILE
        {
            std::string info;

            for (int i = 0; i < client_count; i++)
            {
                info += "--------------------------------------------------------\n";
                info += "CLIENT" + std::to_string(i + 1) + "\n";
                info += clients[i].get_orders_as_string();
                info += "--------------------------------------------------------\n";
            }

            std::string output_file_path = "order_books.txt";

            llfix::FileSystemUtilities::delete_file_if_exists(output_file_path);
            llfix::FileSystemUtilities::append_text_to_file(output_file_path, info);

            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Saved into " + output_file_path + "\n");
        }
        else if (user_input[0] == '3') // ORDERS FROM ALL CLIENTS
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Please enter order count and press enter :");
            int order_count{ 0 };
            std::string string_order_count;
            std::cin >> string_order_count;

            try
            {
                order_count = std::stoi(string_order_count);

                if (order_count <= 0)
                {
                    llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a positive order count\n");
                }
                else
                {
                    Task* new_task = new Task;
                    new_task->task_type = TaskType::SEND_BULK_ORDER;
                    new_task->order_count = order_count;

                    for (int i = 0; i < client_count; i++)
                    {
                        task_queues[i].push(new_task);
                    }

                    llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Task added to all task queues\n");
                }
            }
            catch (...)
            {
                llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a positive order count\n");
            }
        }
        else if (user_input[0] == '4') // RECONNECTIONS TEST
        {
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Please enter clients to reconnect count and press enter :");
            int clients_to_disconnect_count{ 0 };
            std::string string_client_to_disconnect_count;
            std::cin >> string_client_to_disconnect_count;

            try
            {
                clients_to_disconnect_count = std::stoi(string_client_to_disconnect_count);

                if (clients_to_disconnect_count >= client_count)
                {
                    clients_to_disconnect_count = client_count;
                }

                if (clients_to_disconnect_count <= 0)
                {
                    llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a positive clients to reconnect count\n");
                }
                else
                {
                    Task* new_task = new Task;
                    new_task->task_type = TaskType::DISCONNECT;

                    for (int i = 0; i < clients_to_disconnect_count; i++)
                    {
                        task_queues[client_count-1-i].push(new_task);
                    }

                    llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Task added to the queues\n");
                }
            }
            catch (...)
            {
                llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a positive clients to reconnect count\n");
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        }

        if (is_exiting.load() == true)
        {
            break;
        }
    }

    for (int i = 0; i < client_count; i++)
    {
        threads[i]->join();
        clients[i].shutdown(false);
        delete threads[i];
    }

    #if _WIN32
    std::system("pause");
    #endif

    return 0;
}