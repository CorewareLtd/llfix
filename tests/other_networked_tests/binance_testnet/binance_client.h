#pragma once

#include <cstdint>
#include <string>

#include <llfix/engine.h>
#include <llfix/core/ssl/tcp_connector_ssl.h>
#include <llfix/fix_client.h>
#include <llfix/fix_constants.h>

#include "binance_utils.h"

#include <llfix/core/os/console.h>

class BinanceClient : public llfix::FixClient<llfix::TCPConnectorSSL>
{
    private:
        uint32_t m_order_id = 0;
        uint32_t m_query_id = 0;
        std::string m_ed25519_private_pem_path = "ed25519_private.pem";
    public:

        BinanceClient()
        {
        }

        void set_ed25519_private_pem_path(const std::string& path)
        {
            m_ed25519_private_pem_path = path;
        }

        void on_connection() override
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Connection established\n");
        }

        void on_disconnection() override
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Connection lost\n");
        }

        void on_execution_report(const llfix::IncomingFixMessage* message) override
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Exec report : " + message->to_string() + "\n");
        }

        bool send_logon_request() override
        {
            auto logon_request = outgoing_message_instance();
            logon_request->set_msg_type(llfix::FixConstants::MSG_TYPE_LOGON);

            // TAG 98 ENCRYPTION METHOD
            logon_request->set_tag(llfix::FixConstants::TAG_ENCRYPT_METHOD, 0);
            
            // TAG 108 HEARTBEAT INTERVAL
            logon_request->set_tag(llfix::FixConstants::TAG_HEART_BT_INT, static_cast<uint32_t>(get_session()->get_heartbeart_interval_in_nanoseconds() / 1'000'000'000));

            // TAG 1137 DEFAULT APP VER ID
            auto default_app_ver_id = get_session()->get_default_app_ver_id();

            if (default_app_ver_id.length() > 0)
            {
                logon_request->set_tag(llfix::FixConstants::TAG_DEFAULT_APPL_VER_ID, default_app_ver_id);
            }

            // TAG 141 RESET SEQ NOS
            if (get_session()->logon_reset_sequence_numbers_flag())
            {
                get_session()->get_sequence_store()->reset_numbers();
                logon_request->set_tag(llfix::FixConstants::TAG_RESET_SEQ_NUM_FLAG, llfix::FixConstants::FIX_BOOLEAN_TRUE);
            }

            // TAG 553 SESSION USER NAME
            auto session_username = get_session()->get_username();

            if (session_username.length() > 0)
            {
                logon_request->set_tag(llfix::FixConstants::TAG_USERNAME, session_username);
            }

            // TAG 95 AND 96
            std::string signature = get_binance_logon_signature(m_ed25519_private_pem_path, "A", get_session()->get_compid(), get_session()->get_target_compid(), 1, get_session()->get_outgoing_fix_message()->get_sending_time());
            logon_request->set_tag(95, signature.size());
            logon_request->set_binary_tag(96, signature.c_str(), signature.size());

            // TAG 25035 MessageHandling	INT	Possible values:1 - UNORDERED 2 - SEQUENTIAL
            logon_request->set_tag(25035, 1);

            return send_outgoing_message(logon_request);
        }

        bool send_outgoing_message(llfix::OutgoingFixMessage* message) override
        {
            // Setting a header tag
            message->set_tag<llfix::FixMessageComponent::HEADER>(25000, 60000);
            return llfix::FixClient<llfix::TCPConnectorSSL>::send_outgoing_message(message);
        }

        template <bool is_buy>
        void send_new_order(const std::string& symbol, double qty, std::size_t qty_decimal_points, int price)
        {
            auto message = outgoing_message_instance();
            m_order_id++;

            // MSG TYPE
            message->set_msg_type('D');

            // CLORDID
            message->set_tag(11, m_order_id);

            // RIC SYMBOL
            message->set_tag(55, symbol);

            // SIDE
            if constexpr(is_buy)
            {
                message->set_tag(54, '1');
            }
            else
            {
                message->set_tag(54, '2');
            }

            // QUANTITY
            message->set_tag(38, qty, qty_decimal_points);

            // PRICE
            message->set_tag(44, price);

            // ORDER TYPE = LIMIT
            message->set_tag(40, '2');

            // TIME IN FORCE = GOOD TILL CANCEL
            message->set_tag(59, '1');

            // TRANSACT TIME, BINANCE TESTNET DOESN'T NEED IT
            // message->set_timestamp_tag(60);

            // SEND
            send_outgoing_message(message);
        }

        // https://developers.binance.com/docs/binance-spot-api-docs/fix-api#limitquery
        void send_query_limits()
        {
            auto message = outgoing_message_instance();
            m_query_id++;

            message->set_msg_type("XLQ");
            message->set_tag(6136, m_query_id);

            send_outgoing_message(message);
        }

        void on_custom_message_type(const llfix::IncomingFixMessage* message) override
        {
            auto msg_type = message->get_tag_value_as<std::string>(35);

            if (msg_type == "XLR")
            {
                on_query_limits_response(message);
            }
            else
            {
                llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Custom message : " + message->to_string() + "\n");
            }
        }

        void on_query_limits_response(const llfix::IncomingFixMessage* message)
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Query limits response : " + message->to_string() + "\n");

            auto xlr_group_count = message->get_repeating_group_tag_value_as<std::size_t>(25003, 0);

            for (std::size_t i = 0; i < xlr_group_count; i++)
            {
                auto t25004 = message->get_repeating_group_tag_value_as<std::string>(25004, i);
                auto t25005 = message->get_repeating_group_tag_value_as<std::string>(25005, i);
                auto t25006 = message->get_repeating_group_tag_value_as<std::string>(25006, i);
                auto t25007 = message->get_repeating_group_tag_value_as<std::string>(25007, i);

                llfix::Console::print_colour(llfix::ConsoleColour::FG_GREEN, "Group " + std::to_string(i) + " : " + t25004 + " " + t25005 + " " + t25006 + " " + t25007 + "\n");
            }
        }
};