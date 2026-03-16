///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#define LLFIX_ENABLE_DICTIONARY
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <llfix/engine.h>

#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/logger.h>
#include <llfix/core/utilities/tcp_reactor.h>
#include <llfix/fix_server.h>

#include <llfix/core/os/console.h>

#include <cstdint>
#include <string>
#include <iostream>

class TestServer : public llfix::FixServer<llfix::TcpReactor<>>
{
    private:
        uint32_t m_execution_id = 0;
    public:

        virtual void on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(session);
            std::cout << "Incoming logon req : " << message->to_string() << "\n";
        }

        std::string get_all_sessions_display_text()
        {
            auto session_count = get_session_count();
            std::stringstream strm;

            for (std::size_t i = 0; i < session_count; i++)
            {
                std::string current_session_name = "SESSION" + std::to_string(i + 1);
                auto current_session = get_session(current_session_name);

                if (current_session != nullptr)
                {
                    strm << "---------------------------------------------\n";
                    strm << "SESSION : " << current_session_name << "\n";
                    strm << current_session->get_display_text();
                }
            }

            return strm.str();
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

        virtual void on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            auto exec_report = outgoing_message_instance(session);
            exec_report->set_msg_type('8');
            
            log_repeating_group(message);

            // EXEC ID
            m_execution_id++;
            exec_report->set_tag(17, m_execution_id);

            // CLORDID & ORDER ID
            if (message->has_tag(11))
            {
                exec_report->set_tag(11, message->get_tag_value_as<std::string_view>(11));
                exec_report->set_tag(37, message->get_tag_value_as<std::string_view>(11));
            }

            // EXEC TYPE
            exec_report->set_tag(150, '0');

            // ORD STATUS
            exec_report->set_tag(39, '0');

            // TRANSACT TIME
            exec_report->set_timestamp_tag(60);

            // REMAINING QTY
            if (message->has_tag(38))
            {
                auto order_quantity = message->get_tag_value_as<uint32_t>(38);
                exec_report->set_tag(151, order_quantity);
            }

            // CUM QTY
            exec_report->set_tag(14, 0);

            // PX
            if (message->has_tag(44))
            {
                auto order_price = message->get_tag_value_as<uint32_t>(44);
                exec_report->set_tag(31, order_price);
            }

            // AVG PX
            exec_report->set_tag(6, 0);

            // SIDE
            exec_report->set_tag(54, message->get_tag_value_as<char>(54));

            // SYMBOL
            exec_report->set_tag(55, message->get_tag_value_as<std::string_view>(55));

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
            exec_report->set_tag(453, 2);

            exec_report->set_tag(448, "PARTY1");
            exec_report->set_tag(447, 'D');
            exec_report->set_tag(452, 1);

                // Nested
                exec_report->set_tag(802, 2);
                exec_report->set_tag(523, "AAA");
                exec_report->set_tag(803, "888");
                exec_report->set_tag(523, "EEE");
                exec_report->set_tag(803, "777");

            exec_report->set_tag(448, "PARTY2");
            exec_report->set_tag(447, 'D');
            exec_report->set_tag(452, 3);

                // Nested
                exec_report->set_tag(802, 1);
                exec_report->set_tag(523, "GGG");
                exec_report->set_tag(803, "999");

            send_outgoing_message(session, exec_report);
        }
};

int main()
{
    std::string config_file = "config.cfg";

    /////////////////////////////////////////////////////////////////////////////
    llfix::Engine::on_start(config_file);
    TestServer server;

    if (server.create("EXAMPLE_SERVER", config_file) == false)
    {
        std::cout << "Failed to create the FIX server. Check the logs\n";
        return -1;
    }

    if (server.add_sessions_from(config_file) == false)
    {
        std::cout << "Failed to load sessions from " << config_file << ". Check the logs\n";
        return -1;
    }

    // server.specify_repeating_group("D", 453, 448, 447, 452, 802, 523, 803); // Would be necessary without using dictionary
    server.start();

    std::string user_input;

    while(true)
    {
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press i to see the all session info\n");
        llfix::Console::print_colour(llfix::ConsoleColour::FG_RED, "Press q to quit\n");

        std::cin >> user_input;
        user_input = llfix::StringUtilities::to_lower(user_input);

        if (user_input[0] == 'i')
        {
            std::stringstream output;
            output << '\n' << server.get_all_sessions_display_text() << '\n';
            llfix::Console::print_colour(llfix::ConsoleColour::FG_YELLOW, output.str());
        }

        if (user_input[0] == 'q') // QUIT
        {
            break;
        }
    }

    server.shutdown();

    return 0;
}