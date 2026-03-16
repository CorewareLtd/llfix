#ifndef _BENCHMARK_SERVER_H_
#define _BENCHMARK_SERVER_H_

#include <cstdint>
#include <string>
#include <sstream>

#include <llfix/fix_server.h>

#ifndef ENABLE_SCALABLE_SERVER
#include <llfix/core/utilities/tcp_reactor.h>
class BenchmarkServer : public llfix::FixServer<llfix::TcpReactor<>>
#else
#include <llfix/core/utilities/tcp_reactor_scalable.h>
class BenchmarkServer : public llfix::FixServer<llfix::TcpReactorScalable<>>
#endif
{
    private:
        uint32_t m_execution_id = 0;
    public:

        virtual void on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            auto clorid = message->get_tag_value_as<std::string>(11);

            auto exec_report = outgoing_message_instance(session);
            exec_report->set_msg_type('8');

            m_execution_id++;
            exec_report->set_tag(17, m_execution_id);

            exec_report->set_tag(11, clorid);
            exec_report->set_tag(37, clorid);

            exec_report->set_tag(150, '0');
            exec_report->set_tag(39, 0);

            exec_report->set_tag(14, 0);
            exec_report->set_tag(151, message->get_tag_value_as<uint32_t>(38));

            exec_report->set_tag(54, message->get_tag_value_as<char>(54));
            exec_report->set_tag(55, message->get_tag_value_as<std::string>(55));

            exec_report->set_timestamp_tag(60);

            exec_report->set_tag(453, 2);
            exec_report->set_tag(448, "PARTY1");
            exec_report->set_tag(447, 'D');
            exec_report->set_tag(452, 1);
            exec_report->set_tag(448, "PARTY2");
            exec_report->set_tag(447, 'D');
            exec_report->set_tag(452, 3);

            send_outgoing_message(session, exec_report);
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
};

#endif