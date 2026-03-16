#ifndef _SAMPLE_CLIENT_H_
#define _SAMPLE_CLIENT_H_

#include <cstdint>
#include <sstream>


#include <llfix/fix_client.h>
#include <llfix/core/utilities/logger.h>

#include "order.h"

#ifndef LLFIX_ENABLE_TCPDIRECT
#include <llfix/core/utilities/tcp_connector.h>
class SampleClient : public llfix::FixClient<llfix::TCPConnector>
#else
#include <llfix/core/solarflare_tcpdirect/tcp_connector_tcpdirect.h>
class SampleClient : public llfix::FixClient<llfix::TCPConnectorTCPDirect>
#endif
{
    private:
        uint32_t m_order_id = 0;

    public:

        SampleClient()
        {
            specify_repeating_group("8", 453, 448, 447, 452);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // FIX exec reports
        void on_execution_report(const llfix::IncomingFixMessage* message) override
        {
        }

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

            // REPEATING GROUP
            message->set_tag(453, 2);
            message->set_tag(448, "PARTY1");
            message->set_tag(447, 'D');
            message->set_tag(452, 1);
            message->set_tag(448, "PARTY2");
            message->set_tag(447, 'D');
            message->set_tag(452, 3);

            // TRANSACT TIME
            message->set_timestamp_tag(60);

            // SEND
            send_outgoing_message(message);
        }
};

#endif