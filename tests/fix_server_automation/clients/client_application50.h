#pragma once

#include <atomic>
#include <iostream>

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Session.h"

#include "quickfix/fix50/NewOrderSingle.h"
#include "quickfix/fix50/OrderCancelReplaceRequest.h"
#include "quickfix/fix50/OrderCancelRequest.h"

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

#include "../common.h"

class ClientApplication50 : public FIX::Application, public FIX::MessageCracker
{
public:

    void run()
    {
        while (true)
        {
            if (m_logged_on.load() == true)
            {
                break;
            }
        }

        // NEW ORDERS, ORDER IDS 1 TO 1000
        for (std::size_t i = 0; i < NEW_ORDER_COUNT; i++)
        {
            send_new_order();
        }

        // 2ND SET OF 1000 NEW ORDERS
        for (std::size_t i = 0; i < NEW_ORDER_COUNT; i++)
        {
            send_new_order();
        }

        // REPLACE ORDERS
        for (std::size_t i = 0; i < NEW_ORDER_COUNT; i++)
        {
            send_replace_order(NEW_ORDER_COUNT+i+1);
        }

        // CANCEL ORDERS
        for (std::size_t i = 0; i < NEW_ORDER_COUNT; i++)
        {
            send_cancel_order(NEW_ORDER_COUNT + i + 1);
        }

        while (true)
        {
        }
    }

    void send_new_order()
    {
        try
        {
            client_order_id++;

            FIX50::NewOrderSingle newOrder(
                FIX::ClOrdID(std::to_string(client_order_id)),
                FIX::Side(FIX::Side_BUY),
                FIX::TransactTime(),
                FIX::OrdType(FIX::OrdType_LIMIT)
            );

            newOrder.set(FIX::Symbol("BMWG.DE"));
            newOrder.set(FIX::OrderQty(100));
            newOrder.set(FIX::Price(150.00));
            newOrder.set(FIX::TimeInForce(FIX::TimeInForce_DAY));

            FIX::Session::sendToTarget(newOrder, m_sessionID);
        }
        catch (std::exception& e)
        {
            std::cout << "Message Not Sent: " << e.what();
        }
    }

    void send_replace_order(std::size_t orig_order_id)
    {
        try
        {
            client_order_id++;

            FIX50::OrderCancelReplaceRequest replaceOrder(
                FIX::OrigClOrdID(std::to_string(orig_order_id)),   // original order ID
                FIX::ClOrdID(std::to_string(client_order_id)),     // new unique ID
                FIX::Side(FIX::Side_BUY),
                FIX::TransactTime(),
                FIX::OrdType(FIX::OrdType_LIMIT)
            );

            replaceOrder.set(FIX::Symbol("BMWG.DE"));
            replaceOrder.set(FIX::OrderQty(200));   // new quantity
            replaceOrder.set(FIX::Price(150.00));
            replaceOrder.set(FIX::TimeInForce(FIX::TimeInForce_DAY));

            FIX::Session::sendToTarget(replaceOrder, m_sessionID);
        }
        catch (std::exception& e)
        {
            std::cout << "Message Not Sent: " << e.what();
        }
    }

    void send_cancel_order(std::size_t orig_order_id)
    {
        try
        {
            client_order_id++;

            FIX50::OrderCancelRequest cancelOrder(
                FIX::OrigClOrdID(std::to_string(orig_order_id)),   // original order ID
                FIX::ClOrdID(std::to_string(client_order_id)),     // new unique ID
                FIX::Side(FIX::Side_BUY),
                FIX::TransactTime()
            );

            cancelOrder.set(FIX::Symbol("BMWG.DE"));

            FIX::Session::sendToTarget(cancelOrder, m_sessionID);
        }
        catch (std::exception& e)
        {
            std::cout << "Message Not Sent: " << e.what();
        }
    }

private:

    std::size_t client_order_id = 0;
    FIX::SessionID m_sessionID;
    std::atomic<bool> m_logged_on = false;


    void onCreate(const FIX::SessionID&) {}
    void toAdmin(FIX::Message&, const FIX::SessionID&) {}
    void fromAdmin(const FIX::Message&, const FIX::SessionID&) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) {}

    void onLogon(const FIX::SessionID& sessionID)
    {
        m_sessionID = sessionID;
        m_logged_on = true;
        std::cout << "logon event\n" ;
    }

    void onLogout(const FIX::SessionID& sessionID)
    {
        std::cout << "logout event\n" ;
        m_logged_on = false;
    }

    void fromApp(const FIX::Message& message, const FIX::SessionID& sessionID) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType)
    {
        crack(message, sessionID);
    }

    void toApp(FIX::Message& message, const FIX::SessionID& sessionID) EXCEPT(FIX::DoNotSend)
    {
        try
        {
            FIX::PossDupFlag possDupFlag;
            message.getHeader().getField(possDupFlag);
            if (possDupFlag) throw FIX::DoNotSend();
        }
        catch (FIX::FieldNotFound&)
        {
        }
    }

    void onMessage(const FIX50::ExecutionReport& report, const FIX::SessionID& sessionID)
    {
        // We need to override otherwise Quickfix rejects incoming execution reports
    }
};