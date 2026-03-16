#ifndef _SAMPLE_CLIENT_H_
#define _SAMPLE_CLIENT_H_

#include <cstdint>
#include <cstddef>
#include <string>

#include <unordered_map>                                 // For orders

#include <llfix/fix_client.h>
#include <llfix/core/utilities/tcp_connector.h>

#include <llfix/core/utilities/spsc_bounded_queue.h> // Lockfree SPSC task queue

#include "order.h"

enum class TaskType
{
    NONE,
    CONNECT,
    NEW_ORDER_BUY,
    NEW_ORDER_SELL,
    CANCEL_ORDER,
    REPLACE_ORDER
};

struct Task
{
    TaskType task_type = TaskType::NONE;
    Order* order = nullptr;
    Order* orig_order = nullptr;
};

// For Solarflare TCPDirect, specialise with llfix::TCPConnectorTCPDirect instead of llfix::TCPConnector
// For SSL,                  specialise with llfix::TCPConnectorSSL       instead of llfix::TCPConnector
class SampleClient : public llfix::FixClient<llfix::TCPConnector>
{
    private:
        uint32_t m_order_id = 0;
        std::unordered_map<uint32_t, Order*> m_orders;
        llfix::SPSCBoundedQueue<Task*> m_task_queue;

    public:

        SampleClient();
        ~SampleClient();

        bool create_task_queue(std::size_t task_capacity=10240);
        void push_task(Task* task_ptr);

        // You don't need to loop as it will be invoked from one
        void run() override;

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // TCP Connection events
        void on_connection() override;
        void on_disconnection() override;

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // FIX Logon & Logout events
        void on_logon_response(const llfix::IncomingFixMessage* message) override;
        void on_logon_reject(const  llfix::IncomingFixMessage* message) override;
        void on_logout_response(const llfix::IncomingFixMessage* message) override;

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // FIX exec reports
        void on_execution_report(const llfix::IncomingFixMessage* message) override;
        void on_new_order_reject(const llfix::IncomingFixMessage* message);
        void on_order_cancel_replace_reject(const llfix::IncomingFixMessage* message) override;
        void on_session_level_reject(const llfix::IncomingFixMessage* message) override;
        void on_application_level_reject(const llfix::IncomingFixMessage* message) override;

        // 35=3 & 35=j
        void on_application_or_session_level_reject(const llfix::IncomingFixMessage* reject_message);

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // NEW ORDER
        template <OrderType order_type = OrderType::LIMIT, OrderSide order_side = OrderSide::BUY, TimeInForce tif = TimeInForce::GFD>
        void send_new_order(Order* order)
        {
            auto message = outgoing_message_instance();
            m_order_id++;

            // MSG TYPE
            message->set_msg_type('D');

            // CLORDID
            message->set_tag(11, m_order_id);

            // RIC SYMBOL
            message->set_tag(55, order->get_symbol());

            // SIDE
            if constexpr (order_side == OrderSide::BUY)
            {
                message->set_tag(54, '1');
            }
            else if constexpr (order_side == OrderSide::SELL)
            {
                message->set_tag(54, '2');
            }

            // QUANTITY
            message->set_tag(38, order->get_remaining_qty());

            // PRICE
            message->set_tag(44, order->get_price());

            // ORDER TYPE
            if constexpr (order_type == OrderType::LIMIT)
            {
                message->set_tag(40, '2');
            }
            else if constexpr (order_type == OrderType::MARKET)
            {
                message->set_tag(40, '1');
            }

            // TIME IN FORCE
            if constexpr (tif == TimeInForce::GFD)
            {
                message->set_tag(59, '0');
            }
            else if constexpr (tif == TimeInForce::GTC)
            {
                message->set_tag(59, '1');
            }
            else if constexpr (tif == TimeInForce::OPG)
            {
                message->set_tag(59, '2');
            }
            else if constexpr (tif == TimeInForce::IOC)
            {
                message->set_tag(59, '3');
            }
            else if constexpr (tif == TimeInForce::FOK)
            {
                message->set_tag(59, '4');
            }

            // TRANSACT TIME
            message->set_timestamp_tag(60);

            // REPEATING GROUP
            message->set_tag(453, 2);
            message->set_tag(448, "PARTY1");
            message->set_tag(447, 'D');
            message->set_tag(452, 1);
            message->set_tag(448, "PARTY2");
            message->set_tag(447, 'D');
            message->set_tag(452, 3);

            // SEND
            send_outgoing_message(message);

            // ORDER TRACKING
            order->set_numeric_order_id(m_order_id);
            order->set_side(order_side);
            order->set_type(order_type);
            order->set_state(OrderState::PENDING_NEW_ORDER);
            m_orders[m_order_id] = order;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // REPLACE ORDER
        void send_replace_order(Order* replace_order, Order* orig_order);

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // CANCEL ORDER
        void send_cancel_order(Order* original_order);

        ///////////////////////////////////////////////////////////////////
        // OTHERS
        Order* get_order(uint32_t order_id);
        std::string get_orders_as_string();
};

#endif