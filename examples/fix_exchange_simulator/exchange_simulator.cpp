#include <llfix/common.h>

#include "exchange_simulator.h"

#include <string_view>
#include <sstream>

#include <llfix/core/compiler/unused.h>
#include <llfix/fix_protocol_version.h>
#include <llfix/core/utilities/logger.h>
#include <llfix/core/os/console.h>

void ExchangeSimulator::on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    LLFIX_UNUSED(session);
    llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Incoming logon req : " + message->to_string() + "\n");
}

bool ExchangeSimulator::authenticate_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    LLFIX_UNUSED(session);
    LLFIX_UNUSED(message);
    // You can implement custom logon handling logic here.
    // See tests/other_networked_tests/logon_password_authentication for an example.
    return true;
}

void ExchangeSimulator::on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    SimulatorExecReport exec_report;
    exec_report.clorid = message->get_tag_value_as<std::string_view>(11);
    exec_report.symbol = message->get_tag_value_as<std::string_view>(55);
    exec_report.side = message->get_tag_value_as<char>(54);

    auto order_quantity = message->get_tag_value_as<uint32_t>(38);
    auto order_price = message->get_tag_value_as<uint32_t>(44);

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
    m_stats[get_session_name(session)].new_order_count++;
}

void ExchangeSimulator::on_replace_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    SimulatorExecReport exec_report;
    exec_report.clorid = message->get_tag_value_as<std::string_view>(11);
    exec_report.orig_clordid = message->get_tag_value_as<std::string_view>(41);
    exec_report.symbol = message->get_tag_value_as<std::string_view>(55);
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

void ExchangeSimulator::on_cancel_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    SimulatorExecReport exec_report;
    exec_report.clorid = message->get_tag_value_as<std::string_view>(11);
    exec_report.orig_clordid = message->get_tag_value_as<std::string_view>(41);
    exec_report.symbol = message->get_tag_value_as<std::string_view>(55);
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

void ExchangeSimulator::send_execution_report(llfix::FixSession* session, const SimulatorExecReport& contents)
{
    auto exec_report = outgoing_message_instance(session);
    exec_report->set_msg_type('8');

    // EXEC ID
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

void ExchangeSimulator::on_application_level_reject(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    LLFIX_LOG_ERROR("Application level reject received for session " + session->get_name() + " : " + message->to_string());
    on_application_or_session_level_reject(message);
}

void ExchangeSimulator::on_session_level_reject(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    LLFIX_LOG_ERROR("Session level reject received for session " + session->get_name() + " : " + message->to_string());
    on_application_or_session_level_reject(message);
}

// 35=3 & 35=j
void ExchangeSimulator::on_application_or_session_level_reject(const llfix::IncomingFixMessage* reject_message)
{
    if (reject_message->has_tag(58))
    {
        auto reject_reason = reject_message->get_tag_value_as<std::string_view>(58);
        LLFIX_LOG_ERROR(std::string("Reject reason : ") + std::string(reject_reason));
    }

    if (reject_message->has_tag(45))
    {
        auto seq_no = reject_message->get_tag_value_as<std::string_view>(45);
        LLFIX_LOG_ERROR(std::string("Reject seq no : ") + std::string(seq_no));
    }

    if (reject_message->has_tag(371))
    {
        auto invalid_tag = reject_message->get_tag_value_as<std::string_view>(371);
        LLFIX_LOG_ERROR(std::string("Reject invalid tag : ") + std::string(invalid_tag));
    }

    if (reject_message->has_tag(372))
    {
        auto ref_msg_type = reject_message->get_tag_value_as<std::string_view>(372);
        LLFIX_LOG_ERROR(std::string("Reject ref msg type : ") + std::string(ref_msg_type));
    }

    if (reject_message->has_tag(373))
    {
        auto reason = reject_message->get_tag_value_as<std::string_view>(373);
        LLFIX_LOG_ERROR(std::string("Session reject reason : ") + std::string(reason));
    }
}

SimulatorMode ExchangeSimulator::mode() const { return m_mode; }
void ExchangeSimulator::set_mode(SimulatorMode mode) { m_mode = mode; }

std::string ExchangeSimulator::get_all_sessions_display_text()
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

void ExchangeSimulator::init_stats_per_session()
{
    for (const auto& session_entry : m_sessions)
    {
        auto session_name = session_entry.first;
        m_stats.insert({session_name, SimulatorStats()});
    }
}

std::string ExchangeSimulator::get_all_sessions_stats_display_text()
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