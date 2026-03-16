#ifndef _SAMPLE_CLIENT_H_
#define _SAMPLE_CLIENT_H_

#include <cstdint>
#include <unordered_map>
#include <sstream>

#include <llfix/fix_client.h>
#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/logger.h>

#include "order.h"

#ifdef LLFIX_ENABLE_TCPDIRECT
#include <llfix/core/solarflare_tcpdirect/tcp_connector_tcpdirect.h>
class SampleClient : public llfix::FixClient<llfix::TCPConnectorTCPDirect>
#else
#ifndef LLFIX_ENABLE_OPENSSL
#include <llfix/core/utilities/tcp_connector.h>
class SampleClient : public llfix::FixClient<llfix::TCPConnector>
#else
#include <llfix/core/ssl/tcp_connector_ssl.h>
class SampleClient : public llfix::FixClient<llfix::TCPConnectorSSL>
#endif
#endif
{
    private:
        uint32_t m_order_id = 0;
        std::unordered_map<uint32_t, Order*> m_orders;

    public:

        SampleClient()
        {
            specify_repeating_group("8", 453, 448, 447, 452);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // TCP Connection events
        void on_connection() override
        {
            LLFIX_LOG_INFO("Connection established");
        }

        void on_disconnection() override
        {
            LLFIX_LOG_INFO("Connection lost");
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // FIX Logon & Logout events
        void on_logon_response(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(message);
            LLFIX_LOG_INFO("Logon response success");
        }

        void on_logon_reject(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(message);
            LLFIX_LOG_INFO("Logon reject");
        }

        void on_logout_response(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(message);
            LLFIX_LOG_INFO("Logout message received from server");
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // FIX session/admin message events
        void on_server_heartbeat() override
        {
        }

        void on_server_test_request(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(message);
            LLFIX_LOG_INFO("Server test request");
        }

        void on_server_resend_request(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(message);
            LLFIX_LOG_INFO("Server resend request");
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        // FIX exec reports
        void on_execution_report(const llfix::IncomingFixMessage* message) override
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
            else if (exec_type == '1' || exec_type == '2'|| exec_type == 'F')
            {
                auto fill_qty = message->get_tag_value_as<double>(32, 4);

                auto execution_price = message->get_tag_value_as<double>(31, 4);

                m_orders[order_index]->process_execution(static_cast<uint32_t>(fill_qty), static_cast<uint32_t>(execution_price));
            }
            // REPLACE ORDER ACK
            else if (exec_type == '5')
            {
                auto orig_order_id = message->get_tag_value_as<uint32_t>(41);

                if(m_orders.find(orig_order_id) != m_orders.end())
                {
                    m_orders[orig_order_id]->set_state(OrderState::INVALID);

                    auto remaining_qty = message->get_tag_value_as<double>(151, 4);

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

        void on_new_order_reject(const llfix::IncomingFixMessage* message)
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

        void on_order_cancel_replace_reject(const llfix::IncomingFixMessage* message) override
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

        void on_session_level_reject(const llfix::IncomingFixMessage* message) override
        {
            on_application_or_session_level_reject(message);
        }

        void on_application_level_reject(const llfix::IncomingFixMessage* message) override
        {
            on_application_or_session_level_reject(message);
        }

        // 35=3 & 35=j
        void on_application_or_session_level_reject(const llfix::IncomingFixMessage* reject_message)
        {
            if (reject_message->get_tag_value_as<char>(35) == 'j')
            {
                LLFIX_LOG_ERROR("Application level reject received.");
            }
            else
            {
                LLFIX_LOG_ERROR("Session level reject received.");
            }

            if (reject_message->has_tag(58))
            {
                auto reject_reason = reject_message->get_tag_value_as<std::string>(58);
                LLFIX_LOG_ERROR(std::string("Reject reason : ") + reject_reason);
            }

            if (reject_message->has_tag(45))
            {
                auto seq_no = reject_message->get_tag_value_as<std::string>(45);
                LLFIX_LOG_ERROR( std::string("Reject seq no : ") + seq_no);
            }

            if (reject_message->has_tag(371))
            {
                auto invalid_tag = reject_message->get_tag_value_as<std::string>(371);
                LLFIX_LOG_ERROR(std::string("Reject invalid tag : ") + invalid_tag);
            }

            if (reject_message->has_tag(372))
            {
                auto ref_msg_type = reject_message->get_tag_value_as<std::string>(372);
                LLFIX_LOG_ERROR( std::string("Reject ref msg type : ") + ref_msg_type);
            }

            if (reject_message->has_tag(373))
            {
                auto reason = reject_message->get_tag_value_as<std::string>(373);
                LLFIX_LOG_ERROR(std::string("Session reject reason : ") + reason);
            }
        }

        void on_custom_message_type(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_LOG_INFO(std::string("Unrecognised message : ") + message->to_string());
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

            // ORDER TRACKING
            order->set_numeric_order_id(m_order_id);
            order->set_side(order_side);
            order->set_type(order_type);
            order->set_state(OrderState::PENDING_NEW_ORDER);
            m_orders[m_order_id] = order;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // REPLACE ORDER
        void send_replace_order(Order* replace_order, Order* orig_order)
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
        void send_cancel_order(Order* original_order)
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
        Order* get_order(uint32_t order_id)
        {
            return m_orders[order_id];
        }

        std::string get_orders_as_string()
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
};

#endif