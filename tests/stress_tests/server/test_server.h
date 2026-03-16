#ifndef _TEST_SERVER_H_
#define _TEST_SERVER_H_

#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>

#include <llfix/fix_protocol_version.h>
#include <llfix/fix_server.h>
#include <llfix/core/utilities/logger.h>
#include <llfix/core/compiler/unused.h>

///////////////////////////////////////////
// GENERATED CODE
#include "fields/all_fields.h"
#include "fields/MsgType.h"
#include "fields/OrdStatus.h"
#include "fields/PartyRole.h"

#include "messages/ExecutionReport.h"
#include "messages/NewOrderSingle.h"
///////////////////////////////////////////

enum class SimulatorMode
{
    NO_FILLS,
    FILL_NEW_ORDER_ONE_BY_ONE,
    FILL_NEW_ORDER_AT_ONCE,
    FILL_REPLACE_ORDER_ONE_BY_ONE,
    FILL_REPLACE_ORDER_AT_ONCE,
    REJECT_ALL_REQUESTS
};

struct SimulatorExecReport
{
    std::string clorid;
    std::string orig_clordid;
    char exec_type=0;
    uint32_t ord_status = 0;

    int fill_qty = -1;
    int remaining_qty = -1;
    int cum_qty = -1;
    int price = -1;

    int average_price = 0;

    char side = (char)(0);
    std::string symbol;

    bool include_repeating_group = true;
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
class TestServer : public llfix::FixServer<llfix::TcpReactor<>>
#else
#ifndef LLFIX_ENABLE_OPENSSL
#include <llfix/core/utilities/tcp_reactor_scalable.h>
class TestServer : public llfix::FixServer<llfix::TcpReactorScalable<>>
#else
#include <llfix/core/ssl/tcp_reactor_scalable_ssl.h>
class TestServer : public llfix::FixServer<llfix::TcpReactorScalableSSL<>>
#endif
#endif
{
    private:
        SimulatorMode m_mode = SimulatorMode::FILL_NEW_ORDER_AT_ONCE;
        uint32_t m_execution_id = 0;
        std::unordered_map<std::string, SimulatorStats> m_stats; // Per session
    public:

        virtual void on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(session);
            LLFIX_UNUSED(message);
            // std::cout << "Incoming logon req : " << message->to_string() << "\n";
        }

        virtual void on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            custom::FIX44::NewOrderSingle incoming_new_order(message);

            SimulatorExecReport exec_report;

            exec_report.clorid = incoming_new_order.get_ClOrdID();
            exec_report.symbol = incoming_new_order.get_Symbol();
            exec_report.side = incoming_new_order.get_Side();

            auto order_quantity = incoming_new_order.get_OrderQty_as<int>();
            auto order_price = incoming_new_order.get_Price_as<int>();

            if (m_mode == SimulatorMode::NO_FILLS)
            {
                exec_report.exec_type = '0';
                exec_report.ord_status = 0;
                exec_report.cum_qty = 0;
                exec_report.remaining_qty = order_quantity;
                send_execution_report(session, exec_report);
            }
            else if (m_mode == SimulatorMode::FILL_NEW_ORDER_AT_ONCE)
            {
                exec_report.exec_type = '2';
                exec_report.fill_qty = order_quantity;
                exec_report.cum_qty = order_quantity;
                exec_report.remaining_qty = 0;
                exec_report.ord_status = 2;
                exec_report.price = order_price;
                send_execution_report(session, exec_report);
            }
            else if (m_mode == SimulatorMode::FILL_NEW_ORDER_ONE_BY_ONE)
            {
                exec_report.price = order_price;
                exec_report.fill_qty = 1;

                for (int i = 0; i < order_quantity-1; i++)
                {
                    exec_report.exec_type = '1';
                    exec_report.ord_status = 1;
                    exec_report.remaining_qty = order_quantity - i - 1;
                    exec_report.cum_qty = i+1;
                    send_execution_report(session, exec_report);
                }

                exec_report.exec_type = '2';
                exec_report.ord_status = 2;
                exec_report.remaining_qty = 0;
                exec_report.cum_qty = order_quantity;
                send_execution_report(session, exec_report);
            }
            else if (m_mode == SimulatorMode::REJECT_ALL_REQUESTS)
            {
                exec_report.exec_type = '8';
                exec_report.ord_status = 8;
                exec_report.cum_qty = 0;
                exec_report.remaining_qty = 0;
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
            auto order_price = message->get_tag_value_as<uint32_t>(44);

            if (m_mode == SimulatorMode::NO_FILLS)
            {
                exec_report.exec_type = '5';
                exec_report.ord_status = 5;
                exec_report.cum_qty = 0;
                exec_report.remaining_qty = order_quantity;
                send_execution_report(session, exec_report);
            }
            else if (m_mode == SimulatorMode::FILL_REPLACE_ORDER_AT_ONCE)
            {
                exec_report.exec_type = '2';
                exec_report.ord_status = 2;
                exec_report.price = order_price;
                exec_report.fill_qty = order_quantity;
                exec_report.cum_qty = order_quantity;
                exec_report.remaining_qty = 0;
                send_execution_report(session, exec_report);
            }
            else if (m_mode == SimulatorMode::FILL_REPLACE_ORDER_ONE_BY_ONE)
            {
                exec_report.price = order_price;
                exec_report.fill_qty = 1;

                for (uint32_t i = 0; i < order_quantity-1; i++)
                {
                    exec_report.exec_type = '1';
                    exec_report.ord_status = 1;
                    exec_report.remaining_qty = order_quantity - i - 1;
                    exec_report.cum_qty = i+1;
                    send_execution_report(session, exec_report);
                }

                exec_report.exec_type = '2';
                exec_report.ord_status = 2;
                exec_report.remaining_qty = 0;
                exec_report.cum_qty = order_quantity;
                send_execution_report(session, exec_report);
            }
            else if (m_mode == SimulatorMode::REJECT_ALL_REQUESTS)
            {
                exec_report.exec_type = '8';
                exec_report.ord_status = 8;
                exec_report.cum_qty = 0;
                exec_report.remaining_qty = 0;
                send_execution_report(session, exec_report);
            }

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

            exec_report.cum_qty = 0;
            exec_report.remaining_qty = 0;

            if (m_mode != SimulatorMode::REJECT_ALL_REQUESTS)
            {
                exec_report.exec_type = '4';
                exec_report.ord_status = 4;
                send_execution_report(session, exec_report);
            }
            else
            {
                exec_report.exec_type = '8';
                exec_report.ord_status = 8;
                send_execution_report(session, exec_report);
            }

            // Track stats
            m_stats[get_session_name(session)].cancel_order_count++;
        }

        void send_execution_report(llfix::FixSession* session, const SimulatorExecReport& contents)
        {
            custom::FIX44::ExecutionReport execution_report(outgoing_message_instance(session));

            execution_report.set_msg_type(custom::FIX44::MsgType::EXECUTION_REPORT);

            // EXEC ID
            m_execution_id++;
            execution_report.set_ExecID(std::to_string(m_execution_id));

            // CLORDID & ORDER ID
            execution_report.set_OrderID(contents.clorid);
            execution_report.set_ClOrdID(contents.clorid);

            // ORIG ORD ID IF SPECIFIED
            if (contents.orig_clordid.length() > 0)
            {
                execution_report.set_OrigClOrdID(contents.orig_clordid);
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

            execution_report.set_ExecType(exec_type);

            // ORD STATUS
            execution_report.set_OrdStatus(std::to_string(contents.ord_status)[0]);

            // TRANSACT TIME
            execution_report.set_timestamp_TransactTime();

            // FILL QUANTITY IF SPECIFIED
            if (contents.fill_qty >= 0)
            {
                execution_report.set_LastQty(contents.fill_qty, 4);
            }

            // REMAINING QUANTITY IF SPECIFIED
            if (contents.remaining_qty >= 0)
            {
                execution_report.set_LeavesQty(contents.remaining_qty, 4);
            }

            // CUM QUANTITY IF SPECIFIED
            if (contents.cum_qty >= 0)
            {
                execution_report.set_CumQty(contents.cum_qty, 4);
            }

            // FILL PX IF SPECIFIED
            if (contents.price >= 0)
            {
                execution_report.set_LastPx(static_cast<double>(contents.price), 4);
            }

            // SIDE IF SPECIFIED
            if (contents.side != (char)(0))
            {
                execution_report.set_Side(contents.side);
            }

            // SYMBOL IF SPECIFIED
            if (contents.symbol.length() > 0)
            {
                execution_report.set_Symbol(contents.symbol);
            }

            // AVERAGE PRICE
            execution_report.set_AvgPx(contents.average_price);

            // REPEATING GROUP IF SPECIFIED
            if (contents.include_repeating_group)
            {
                execution_report.set_NoPartyIDs(2);

                execution_report.set_PartyID("PARTY1");
                execution_report.set_PartyIDSource('D');
                execution_report.set_PartyRole((int)custom::FIX44::PartyRole::EXECUTING_FIRM);

                execution_report.set_PartyID("PARTY2");
                execution_report.set_PartyIDSource('D');
                execution_report.set_PartyRole((int)custom::FIX44::PartyRole::CLIENT_ID);
            }

            if(send_outgoing_message(session, execution_report.outgoing_message()) )
            {
                // Track stats
                switch (contents.exec_type)
                {
                    case '0': m_stats[get_session_name(session)].ack_count++; break;
                    case '1': m_stats[get_session_name(session)].part_fill_count++; break;
                    case '2': m_stats[get_session_name(session)].full_fill_count++; break;
                    case '4': m_stats[get_session_name(session)].ack_count++; break;
                    case '5': m_stats[get_session_name(session)].ack_count++; break;
                    case '8': m_stats[get_session_name(session)].reject_count++; break;
                    default: break;
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

        SimulatorMode mode() const { return m_mode; }
        void set_mode(SimulatorMode mode) { m_mode = mode; }

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

                    auto current_stats = m_stats[current_session_name];
                    strm << "New order : " << current_stats.new_order_count << "\n";
                    strm << "Repl order : " << current_stats.replace_order_count << "\n";
                    strm << "Cxl order : " << current_stats.cancel_order_count << "\n";
                    strm << "Ack : " << current_stats.ack_count << "\n";
                    strm << "Part fill : " << current_stats.part_fill_count << "\n";
                    strm << "Full fill : " << current_stats.full_fill_count << "\n";
                    strm << "Reject : " << current_stats.reject_count << "\n";
                }
            }

            return strm.str();
        }
};

#endif