///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define LLFIX_ENABLE_DICTIONARY
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <llfix/engine.h>

#include <llfix/core/utilities/logger.h>
#include <llfix/core/utilities/tcp_connector.h>
#include <llfix/fix_client.h>

#include <llfix/core/os/console.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/utilities/spsc_bounded_queue.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <sstream>
#include <thread>
#include <memory>
#include <iostream>

class SampleClient : public llfix::FixClient<llfix::TCPConnector>
{
    private:
        uint32_t m_order_id = 0;
    public:

        SampleClient()
        {
            // specify_repeating_group("8", 453, 448, 447, 452, 802, 523, 803); // Would be necessary without using dictionary
        }

        void on_connection() override
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Connection established\n");
        }

        void on_disconnection() override
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Connection lost\n");
        }

        void log_repeating_group(const llfix::IncomingFixMessage* message)
        {
            if (message->has_repeating_group_tag(453))
            {
                uint32_t repeating_group_no_parties = message->get_repeating_group_tag_value_as<uint32_t>(453, 0);
                uint32_t total_nested_group_count = 0;

                std::string repeating_group_msg = "Repeating group : ";

                for (std::size_t i = 0; i < repeating_group_no_parties; i++)
                {
                    repeating_group_msg += message->get_repeating_group_tag_value_as<std::string_view>(448, i);
                    repeating_group_msg += " ";
                    repeating_group_msg += message->get_repeating_group_tag_value_as<std::string_view>(447, i);
                    repeating_group_msg += " ";
                    repeating_group_msg += message->get_repeating_group_tag_value_as<std::string_view>(452, i);
                    repeating_group_msg += " ";

                    uint32_t nested_repeating_group_no = message->get_repeating_group_tag_value_as<uint32_t>(802, i);

                    for(std::size_t j=0; j<nested_repeating_group_no; j++)
                    {
                        repeating_group_msg += message->get_repeating_group_tag_value_as<std::string_view>(523, total_nested_group_count + j);
                        repeating_group_msg += " ";
                        repeating_group_msg += message->get_repeating_group_tag_value_as<std::string_view>(803, total_nested_group_count + j);
                        repeating_group_msg += " ";
                    }

                    total_nested_group_count += nested_repeating_group_no;
                }

                LLFIX_LOG_INFO(repeating_group_msg);
            }
        }

        void on_execution_report(const llfix::IncomingFixMessage* message) override
        {
            log_repeating_group(message);
            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Exec report : " + message->to_string() + "\n");
        }

        void send_new_order(bool is_buy, int qty, int price)
        {
            auto message = outgoing_message_instance();
            m_order_id++;

            // MSG TYPE
            message->set_msg_type('D');

            // CLORDID
            message->set_tag(11, m_order_id);

            // RIC SYMBOL
            message->set_tag(55, "NOKIA.HE");

            // SIDE
            if (is_buy)
            {
                message->set_tag(54, '1');
            }
            else
            {
                message->set_tag(54, '2');
            }

            // QUANTITY
            message->set_tag(38, qty);

            // PRICE
            message->set_tag(44, price);

            // ORDER TYPE = LIMIT
            message->set_tag(40, '2');

            // TIME IN FORCE = GFD
            message->set_tag(59, '0');

            // TRANSACT TIME
            message->set_timestamp_tag(60);

            // REPEATING GROUP
            /*
            453=2|
              448=PARTY1|
              447=D|
              452=1|
              802=2|
                523=AAA|
                803=888|
                523=EEE|
                803=777|
              448=PARTY2|
              447=D|
              452=3|
              802=1|
                523=GGG|
                803=999|
            */
            message->set_tag(453, 2);

            message->set_tag(448, "PARTY1");
            message->set_tag(447, 'D');
            message->set_tag(452, 1);

                // Nested
                message->set_tag(802, 2);
                message->set_tag(523, "AAA");
                message->set_tag(803, "888");
                message->set_tag(523, "EEE");
                message->set_tag(803, "777");

            message->set_tag(448, "PARTY2");
            message->set_tag(447, 'D');
            message->set_tag(452, 3);

                // Nested
                message->set_tag(802, 1);
                message->set_tag(523, "GGG");
                message->set_tag(803, "999");

            // SEND
            send_outgoing_message(message);
        }
};

enum class TaskType
{
    NONE,
    SEND_BULK_ORDER,
};

struct Task
{
    TaskType task_type = TaskType::NONE;
    int order_count = 0;
};

llfix::SPSCBoundedQueue<Task*> task_queue;

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

    SampleClient client;

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
    client.shutdown(true);

    connection_success = client.connect();

    if (connection_success == false)
    {
        std::cout << "2nd connection to server failed. Please start the server before runnning this process and check the logs." << std::endl;
        return -1;
    }

    while (true)
    {
        client.process_incoming_messages();

        auto session_state = client.get_session()->get_state();

        if (session_state == llfix::SessionState::LOGGED_ON)
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

                                client.send_new_order(true, 10, 100);
                            }
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
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press 2 to send orders\n");
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

                    task_queue.push(new_task);

                    llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "Task added to all task queues\n");
                }
            }
            catch (...)
            {
                llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, "You need to specify a positive order count\n");

            }
        }
    }

    client_thread->join();

    return 0;
}