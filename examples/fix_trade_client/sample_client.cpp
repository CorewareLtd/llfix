#include <llfix/common.h>

#include "sample_client.h"

#include <string_view>
#include <sstream>
#include <iostream>                                      // Only for example purpose

#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/logger.h>             // Thread safe async or sync logger ( based on engine settings )

SampleClient::SampleClient()
{
    specify_repeating_group("8", 453, 448, 447, 452); // This is not needed when you use dictionaries
}

SampleClient::~SampleClient()
{
    for(auto& order_entry:m_orders)
        if(order_entry.second)
            delete order_entry.second;

    while(1)
    {
        Task* current_task = nullptr;
        if(m_task_queue.try_pop(&current_task)) delete current_task;
        else break;
    }
}

bool SampleClient::create_task_queue(std::size_t task_capacity)
{
    return m_task_queue.create(task_capacity);
}

void SampleClient::push_task(Task* task_ptr)
{
    m_task_queue.push(task_ptr);
}

// You don't need to loop as it will be invoked from one
void SampleClient::run()
{
    if(!m_is_exiting.load())
    {
        process();

        // POP TASK
        Task* current_task = nullptr;
        bool got_new_task = m_task_queue.try_pop(&current_task);

        // EXECUTE THE POPPED TASK
        if (got_new_task)
        {
            auto state = get_session()->get_state();

            if (current_task->task_type == TaskType::CONNECT)
            {
                bool connection_success = connect();

                if (connection_success == true)
                {
                    std::cout << "Connection to server success\n";
                }
            }
            else if (current_task->task_type == TaskType::NEW_ORDER_BUY)
            {
                if (state == llfix::SessionState::LOGGED_ON)
                    send_new_order<OrderType::LIMIT, OrderSide::BUY, TimeInForce::GFD>(current_task->order);
            }
            else if (current_task->task_type == TaskType::NEW_ORDER_SELL)
            {
                if (state == llfix::SessionState::LOGGED_ON)
                    send_new_order<OrderType::LIMIT, OrderSide::SELL, TimeInForce::GFD>(current_task->order);
            }
            else if (current_task->task_type == TaskType::REPLACE_ORDER)
            {
                if (state == llfix::SessionState::LOGGED_ON)
                    send_replace_order(current_task->order, current_task->orig_order);
            }
            else if (current_task->task_type == TaskType::CANCEL_ORDER)
            {
                if (state == llfix::SessionState::LOGGED_ON)
                    send_cancel_order(current_task->orig_order);
            }

            delete current_task;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////
// TCP Connection events
void SampleClient::on_connection()
{
    LLFIX_LOG_INFO("Connection established");
}

void SampleClient::on_disconnection()
{
    LLFIX_LOG_INFO("Connection lost");
}

////////////////////////////////////////////////////////////////////////////////////////////////
// FIX Logon & Logout events
void SampleClient::on_logon_response(const llfix::IncomingFixMessage* message)
{
    LLFIX_UNUSED(message);
    LLFIX_LOG_INFO("Logon response success");
}

void SampleClient::on_logon_reject(const  llfix::IncomingFixMessage* message)
{
    LLFIX_UNUSED(message);
    LLFIX_LOG_INFO("Logon reject");
}

void SampleClient::on_logout_response(const llfix::IncomingFixMessage* message)
{
    LLFIX_UNUSED(message);
    LLFIX_LOG_INFO("Logout message received from server");
}

////////////////////////////////////////////////////////////////////////////////////////////////
// FIX exec reports
void SampleClient::on_execution_report(const llfix::IncomingFixMessage* message)
{
    auto order_id = message->get_tag_value_as<uint32_t>(11);
    uint32_t order_index = static_cast<uint32_t>(order_id);
    auto exec_type = message->get_tag_value_as<char>(150);

    ////////////////////////////////////////
    //// REPEATING GROUPS EXTRACTION
    if (message->has_repeating_group_tag(453))
    {
        uint32_t repeating_group_no_parties = message->get_repeating_group_tag_value_as<uint32_t>(453, 0);

        std::string repeating_group_msg = "Repeating group : ";

        for (std::size_t i = 0; i < repeating_group_no_parties; i++)
        {
            repeating_group_msg += message->get_repeating_group_tag_value_as<std::string_view>(448, i);
            repeating_group_msg += " ";
            repeating_group_msg += message->get_repeating_group_tag_value_as<std::string_view>(447, i);
            repeating_group_msg += " ";
            repeating_group_msg += message->get_repeating_group_tag_value_as<std::string_view>(452, i);
            repeating_group_msg += " ";
        }

        LLFIX_LOG_INFO(repeating_group_msg);
    }
    ////////////////////////////////////////

    // NEW ORDER ACK
    if (exec_type == '0')
    {
        if (m_orders.find(order_index) == m_orders.end())
        {
            LLFIX_LOG_ERROR("Could not find order, order id : " + std::to_string(order_index));
            return;
        }

        m_orders[order_index]->set_state(OrderState::NEW_ORDER);
    }
    // EXECUTION : PARTIAL OR FULL FILL
    else if (exec_type == '1' || exec_type == '2' || exec_type == 'F')
    {
        auto fill_qty = message->get_tag_value_as<uint32_t>(32);

        auto execution_price = message->get_tag_value_as<uint32_t>(31);

        m_orders[order_index]->process_execution(fill_qty, execution_price);
    }
    // REPLACE ORDER ACK
    else if (exec_type == '5')
    {
        auto orig_order_id = message->get_tag_value_as<uint32_t>(41);

        if(m_orders.find(orig_order_id) != m_orders.end())
        {
            m_orders[orig_order_id]->set_state(OrderState::INVALID);

            auto remaining_qty = message->get_tag_value_as<uint32_t>(151);

            m_orders[order_index]->set_remaining_qty(static_cast<uint32_t>(remaining_qty));
            m_orders[order_index]->update_order_state_based_on_quantities();
        }
    }
    // CANCEL ACK
    else if (exec_type == '4')
    {
        auto orig_order_id = message->get_tag_value_as<uint32_t>(41);

        if(m_orders.find(orig_order_id) != m_orders.end())
        {
            m_orders[orig_order_id]->process_cancellation();
        }
    }
    // NEW ORDER REJECT
    else if (exec_type == '8')
    {
        on_new_order_reject(message);
    }
}

void SampleClient::on_new_order_reject(const llfix::IncomingFixMessage* message)
{
    if (message->has_tag(11))
    {
        auto order_index = message->get_tag_value_as<uint32_t>(11);

        auto existing_state = m_orders[order_index]->get_state();

        if (existing_state == OrderState::PENDING_NEW_ORDER)
        {
            m_orders[order_index]->set_state(OrderState::REJECTED_BY_VENUE);
            LLFIX_LOG_ERROR("Received new order reject, order id " + std::to_string(order_index));
        }
    }
}

void SampleClient::on_order_cancel_replace_reject(const llfix::IncomingFixMessage* message)
{
    if (message->has_tag(11))
    {
        auto order_index = message->get_tag_value_as<uint32_t>(11);

        auto existing_state = m_orders[order_index]->get_state();

        if (existing_state == OrderState::PENDING_REPLACE_ORDER)
        {
            m_orders[order_index]->update_order_state_based_on_quantities();
            LLFIX_LOG_ERROR("Received replace order reject, order id " + std::to_string(order_index));
        }
        else if (existing_state == OrderState::PENDING_CANCEL_ORDER)
        {
            m_orders[order_index]->update_order_state_based_on_quantities();
            LLFIX_LOG_ERROR("Received cancel order reject, order id " + std::to_string(order_index));
        }
    }
}

void SampleClient::on_session_level_reject(const llfix::IncomingFixMessage* message)
{
    LLFIX_LOG_ERROR("Session level reject received : " + message->to_string());
    on_application_or_session_level_reject(message);
}

void SampleClient::on_application_level_reject(const llfix::IncomingFixMessage* message)
{
    LLFIX_LOG_ERROR("Application level reject received : " + message->to_string());
    on_application_or_session_level_reject(message);
}

// 35=3 & 35=j
void SampleClient::on_application_or_session_level_reject(const llfix::IncomingFixMessage* reject_message)
{
    if (reject_message->has_tag(58))
    {
        auto reject_reason = reject_message->get_tag_value_as<std::string_view>(58);
        LLFIX_LOG_ERROR(std::string("Reject reason : ") + std::string(reject_reason));
    }

    if (reject_message->has_tag(45))
    {
        auto seq_no = reject_message->get_tag_value_as<std::string_view>(45);
        LLFIX_LOG_ERROR( std::string("Reject seq no : ") + std::string(seq_no));
    }

    if (reject_message->has_tag(371))
    {
        auto invalid_tag = reject_message->get_tag_value_as<std::string_view>(371);
        LLFIX_LOG_ERROR(std::string("Reject invalid tag : ") + std::string(invalid_tag));
    }

    if (reject_message->has_tag(372))
    {
        auto ref_msg_type = reject_message->get_tag_value_as<std::string_view>(372);
        LLFIX_LOG_ERROR( std::string("Reject ref msg type : ") + std::string(ref_msg_type));
    }

    if (reject_message->has_tag(373))
    {
        auto reason = reject_message->get_tag_value_as<std::string_view>(373);
        LLFIX_LOG_ERROR(std::string("Session reject reason : ") + std::string(reason));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// REPLACE ORDER
void SampleClient::send_replace_order(Order* replace_order, Order* orig_order)
{
    auto message = outgoing_message_instance();
    m_order_id++;

    // MSG TYPE
    message->set_msg_type('G');

    // ORIG CLORDID
    message->set_tag(41, replace_order->get_numeric_order_id());

    // CLORDID
    message->set_tag(11, m_order_id);

    // RIC SYMBOL
    message->set_tag(55, replace_order->get_symbol());

    // SIDE
    if (replace_order->get_side() == OrderSide::BUY)
    {
        message->set_tag(54, '1');
    }
    else
    {
        message->set_tag(54, '2');
    }

    // QTY
    message->set_tag(38, replace_order->get_remaining_qty());

    // PRICE
    message->set_tag(44, replace_order->get_price());

    // ORDER TYPE
    auto type = replace_order->get_type();

    if (type == OrderType::LIMIT)
    {
        message->set_tag(40, '2');
    }
    else if (type == OrderType::MARKET)
    {
        message->set_tag(40, '1');
    }

    // TRANSACT TIME
    message->set_timestamp_tag(60);

    // SEND
    send_outgoing_message(message);

    // ORDER TRACKING
    orig_order->set_state(OrderState::PENDING_REPLACE_ORDER);
    replace_order->set_state(OrderState::PENDING_REPLACE_ORDER);
    replace_order->set_numeric_order_id(m_order_id);
    m_orders[m_order_id] = replace_order;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CANCEL ORDER
void SampleClient::send_cancel_order(Order* original_order)
{
    auto message = outgoing_message_instance();
    m_order_id++;

    // MSG TYPE
    message->set_msg_type('F');

    // CLORDID
    message->set_tag(11, m_order_id);

    // ORIG CLORDID
    message->set_tag(41, original_order->get_numeric_order_id());

    // SYMBOL
    message->set_tag(55, original_order->get_symbol());

    // SIDE
    if (original_order->get_side() == OrderSide::BUY)
    {
        message->set_tag(54, '1');
    }
    else
    {
        message->set_tag(54, '2');
    }

    // QTY
    message->set_tag(38, original_order->get_remaining_qty());

    // TRANSACT TIME
    message->set_timestamp_tag(60);

    // SEND
    send_outgoing_message(message);

    // ORDER TRACKING
    original_order->set_state(OrderState::PENDING_CANCEL_ORDER);
}

///////////////////////////////////////////////////////////////////
// OTHERS
Order* SampleClient::get_order(uint32_t order_id)
{
    return m_orders[order_id];
}

std::string SampleClient::get_orders_as_string()
{
    std::stringstream stream;

    for(uint32_t i=1; i<=m_orders.size();i++)
    {
        if (m_orders[i])
        {
            if (m_orders[i]->get_state() != OrderState::INVALID)
            {
                stream << "\n";
                stream << m_orders[i]->get_display_text();
                stream << "\n";
            }
        }
    }

    return stream.str();
}