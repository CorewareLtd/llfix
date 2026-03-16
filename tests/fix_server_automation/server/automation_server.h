#ifndef _AUTOMATION_SERVER_H_
#define _AUTOMATION_SERVER_H_

#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>

#include <llfix/fix_protocol_version.h>
#include <llfix/fix_server.h>
#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/logger.h>

#include "../common.h"

struct SimulatorExecReport
{
    std::string clorid;
    std::string orig_clordid;
    char exec_type='0';
    uint32_t ord_status = 0;

    int fill_qty = -1;
    int remaining_qty = -1;
    int cum_qty = -1;
    int price = -1;
    int average_price = 0;

    char side = (char)(0);
    std::string symbol;

    bool include_repeating_group = false;
};

struct SimulatorStats
{
    uint32_t new_order_count = 0;
    uint32_t replace_order_count = 0;
    uint32_t cancel_order_count = 0;
    uint32_t ack_count = 0;
    uint32_t part_fill_count = 0;
    uint32_t full_fill_count = 0;
    uint32_t reject_count = 0;
};

#ifndef ENABLE_SCALABLE_SERVER
#include <llfix/core/utilities/tcp_reactor.h>
class AutomationServer : public llfix::FixServer<llfix::TcpReactor<>>
#else
#include <llfix/core/utilities/tcp_reactor_scalable.h>
class AutomationServer : public llfix::FixServer<llfix::TcpReactorScalable<>>
#endif
{
    private:
        uint32_t m_execution_id = 0;
        std::unordered_map<std::string, SimulatorStats> m_stats; // Per session
        int m_mode = MODE1;
    public:

        virtual void on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(session);
            std::cout << "Incoming logon req : " << message->to_string() << "\n";
        }

        virtual void on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            SimulatorExecReport exec_report;
            exec_report.clorid = message->get_tag_value_as<std::string>(11);
            exec_report.symbol = message->get_tag_value_as<std::string>(55);
            exec_report.side = message->get_tag_value_as<char>(54);

            auto order_quantity = message->get_tag_value_as<uint32_t>(38);
            auto order_price = message->get_tag_value_as<uint32_t>(44);

            if (m_stats[get_session_name(session)].new_order_count >= NEW_ORDER_COUNT)
            {
                // NO FILLS
                exec_report.exec_type = '0';
                exec_report.ord_status = 0;
                exec_report.cum_qty = 0;
                exec_report.remaining_qty = order_quantity;
                send_execution_report(session, exec_report);
            }
            else
            {
                // FULL FILLS
                exec_report.exec_type = '2';
                exec_report.fill_qty = order_quantity;
                exec_report.cum_qty = order_quantity;
                exec_report.remaining_qty = 0;
                exec_report.ord_status = 2;
                exec_report.price = order_price;
                send_execution_report(session, exec_report);
            }

            // Track stats
            m_stats[get_session_name(session)].new_order_count++;
        }

        virtual void on_replace_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            SimulatorExecReport exec_report;
            exec_report.clorid = message->get_tag_value_as<std::string>(11);
            exec_report.orig_clordid = message->get_tag_value_as<std::string>(41);
            exec_report.symbol = message->get_tag_value_as<std::string>(55);
            exec_report.side = message->get_tag_value_as<char>(54);

            auto order_quantity = message->get_tag_value_as<uint32_t>(38);

            // NO FILLS
            exec_report.exec_type = '5';
            exec_report.ord_status = 5;
            exec_report.cum_qty = 0;
            exec_report.remaining_qty = order_quantity;
            send_execution_report(session, exec_report);

            // Track stats
            m_stats[get_session_name(session)].replace_order_count++;
        }

        virtual void on_cancel_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            SimulatorExecReport exec_report;
            exec_report.clorid = message->get_tag_value_as<std::string>(11);
            exec_report.orig_clordid = message->get_tag_value_as<std::string>(41);
            exec_report.symbol = message->get_tag_value_as<std::string>(55);
            exec_report.side = message->get_tag_value_as<char>(54);

            // cancel ack
            exec_report.exec_type = '4';
            exec_report.ord_status = 4;
            exec_report.cum_qty = 0;
            exec_report.remaining_qty = 0;
            send_execution_report(session, exec_report);

            // Track stats
            m_stats[get_session_name(session)].cancel_order_count++;
        }

        void send_execution_report(llfix::FixSession* session, const SimulatorExecReport& contents)
        {
            auto exec_report = outgoing_message_instance(session);
            exec_report->set_msg_type('8');

            // EXEC ID & ORDER ID
            m_execution_id++;
            exec_report->set_tag(17, m_execution_id);

            // CLORDID & ORDER ID
            exec_report->set_tag(11, contents.clorid);
            exec_report->set_tag(37, contents.clorid);

            // ORIG ORD ID IF SPECIFIED
            if (contents.orig_clordid.length() > 0)
            {
                exec_report->set_tag(41, contents.orig_clordid);
            }

            // EXEC TYPE
            auto exec_type = contents.exec_type;

            if (session->get_protocol_version() > llfix::FixProtocolVersion::FIX42)
            {
                if (exec_type == '1' || exec_type == '2')
                {
                    exec_type = 'F';
                }
            }

            exec_report->set_tag(150, exec_type);

            // ORD STATUS
            exec_report->set_tag(39, contents.ord_status);

            // TRANSACT TIME
            exec_report->set_timestamp_tag(60);

            // FILL QUANTITY IF SPECIFIED
            if (contents.fill_qty >= 0)
            {
                exec_report->set_tag(32, contents.fill_qty);
            }

            // REMAINING QUANTITY IF SPECIFIED
            if (contents.remaining_qty >= 0)
            {
                exec_report->set_tag(151, contents.remaining_qty);
            }

            // CUM QUANTITY IF SPECIFIED
            if (contents.cum_qty >= 0)
            {
                exec_report->set_tag(14, contents.cum_qty);
            }

            // FILL PX IF SPECIFIED
            if (contents.price >= 0)
            {
                exec_report->set_tag(31, contents.price);
            }

            // AVERAGE PRICE
            exec_report->set_tag(6, contents.average_price);

            // SIDE IF SPECIFIED
            if(contents.side != (char)(0))
            {
                exec_report->set_tag(54, contents.side);
            }

            // SYMBOL IF SPECIFIED
            if(contents.symbol.length() > 0)
            {
                exec_report->set_tag(55, contents.symbol);
            }

            // REPEATING GROUP IF SPECIFIED
            if(contents.include_repeating_group)
            {
                exec_report->set_tag(453, 2);
                exec_report->set_tag(448, "PARTY1");
                exec_report->set_tag(447, 'D');
                exec_report->set_tag(452, 1);
                exec_report->set_tag(448, "PARTY2");
                exec_report->set_tag(447, 'D');
                exec_report->set_tag(452, 3);
            }

            send_outgoing_message(session, exec_report);

            // Track stats
            auto session_name = get_session_name(session);
            switch (contents.exec_type)
            {
                case '0': m_stats[session_name].ack_count++; break;
                case '1': m_stats[session_name].part_fill_count++; break;
                case '2': m_stats[session_name].full_fill_count++; break;
                case '4': m_stats[session_name].ack_count++; break;
                case '5': m_stats[session_name].ack_count++; break;
                case '8': m_stats[session_name].reject_count++; break;
                default: break;
            }

            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (m_stats[session_name].ack_count == (NEW_ORDER_COUNT*3))
            {
                if(m_mode == MODE1 || m_mode == MODE2)
                {
                    // TRIGGER CLIENTS' RESEND REQUESTS WHEN WE ARE IN GAP FILL MODE OR REPLAY MODE
                    auto fake_message = outgoing_message_instance(session);
                    fake_message->set_msg_type('0');

                    for (int i = 0; i < 5; i++)
                    {
                        send_fake_outgoing_message(session, fake_message);
                    }
                }
                else if(m_mode == MODE3)
                {
                    // WILL TRIGGER SERVER TO SEND RESEND REQUESTS TO THE CLIENTS
                    auto store = session->get_sequence_store();
                    auto incoming_seq_no = store->get_incoming_seq_no();
                    store->set_incoming_seq_no(incoming_seq_no-5);
                }
            }
        }

        virtual void on_application_level_reject(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            LLFIX_LOG_ERROR("Application level reject received for session " + session->get_name() + " : " + message->to_string());
            on_application_or_session_level_reject(message);
        }

        virtual void on_session_level_reject(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            LLFIX_LOG_ERROR("Session level reject received for session " + session->get_name() + " : " + message->to_string());
            on_application_or_session_level_reject(message);
        }

        // 35=3 & 35=j
        void on_application_or_session_level_reject(const llfix::IncomingFixMessage* reject_message)
        {
            if (reject_message->has_tag(58))
            {
                auto reject_reason = reject_message->get_tag_value_as<std::string>(58);
                LLFIX_LOG_ERROR(std::string("Reject reason : ") + reject_reason);
            }

            if (reject_message->has_tag(45))
            {
                auto seq_no = reject_message->get_tag_value_as<std::string>(45);
                LLFIX_LOG_ERROR(std::string("Reject seq no : ") + seq_no);
            }

            if (reject_message->has_tag(371))
            {
                auto invalid_tag = reject_message->get_tag_value_as<std::string>(371);
                LLFIX_LOG_ERROR(std::string("Reject invalid tag : ") + invalid_tag);
            }

            if (reject_message->has_tag(372))
            {
                auto ref_msg_type = reject_message->get_tag_value_as<std::string>(372);
                LLFIX_LOG_ERROR(std::string("Reject ref msg type : ") + ref_msg_type);
            }

            if (reject_message->has_tag(373))
            {
                auto reason = reject_message->get_tag_value_as<std::string>(373);
                LLFIX_LOG_ERROR(std::string("Session reject reason : ") + reason);
            }
        }

        std::string get_all_sessions_display_text()
        {
            std::stringstream strm;

            for (const auto& session_entry : m_sessions)
            {
                strm << "---------------------------------------------\n";
                strm << "SESSION : " << session_entry.first << "\n";
                strm << session_entry.second->get_display_text();
            }

            return strm.str();
        }

        void init_stats_per_session()
        {
            for (const auto& session_entry : m_sessions)
            {
                auto session_name = session_entry.first;
                m_stats.insert({session_name, SimulatorStats()});
            }
        }

        std::string get_all_sessions_stats_display_text()
        {
            std::stringstream strm;

            for (const auto& session_entry : m_sessions)
            {
                strm << "---------------------------------------------\n";
                strm << "SESSION : " << session_entry.first << "\n";

                auto current_stats = m_stats[session_entry.first];
                strm << "New order : " << current_stats.new_order_count << "\n";
                strm << "Repl order : " << current_stats.replace_order_count << "\n";
                strm << "Cxl order : " << current_stats.cancel_order_count << "\n";
                strm << "Ack : " << current_stats.ack_count << "\n";
                strm << "Part fill : " << current_stats.part_fill_count << "\n";
                strm << "Full fill : " << current_stats.full_fill_count << "\n";
                strm << "Reject : " << current_stats.reject_count << "\n";
            }

            return strm.str();
        }

        void set_mode(int n) { m_mode = n; }
        int get_mode() const { return m_mode; }
};

#endif