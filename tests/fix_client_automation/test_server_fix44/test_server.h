#ifndef _TEST_SERVER_H_
#define _TEST_SERVER_H_

#include <cstddef>
#include <string>

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"

#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix44/NewOrderSingle.h"
#include "quickfix/fix44/OrderCancelRequest.h"
#include "quickfix/fix44/OrderCancelReplaceRequest.h"
#include "quickfix/fix44/OrderCancelReject.h"
#include "quickfix/fix44/Logon.h"

class TestServer : public FIX::Application, public FIX::MessageCracker
{
public:

    TestServer(const std::string& username, const std::string& password, const std::string& fill_instrument, const std::string& reject_instrument)
    {
        m_username = username;
        m_password = password;

        m_fill_instrument = fill_instrument;
        m_reject_instrument = reject_instrument;
    }

    void onCreate(const FIX::SessionID& sessionID)
    {
    }

    void onLogon(const FIX::SessionID& sessionID)
    {
    }

    void onLogout(const FIX::SessionID& sessionID)
    {
    }

    void toAdmin(FIX::Message& message, const FIX::SessionID& sessionID)
    {
    }

    void toApp(FIX::Message& message, const FIX::SessionID& sessionID) EXCEPT(FIX::DoNotSend)
    {
    }

    void fromAdmin(const FIX::Message& message, const FIX::SessionID& sessionID) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon)
    {
        crack(message, sessionID);
    }

    void fromApp(const FIX::Message& message, const FIX::SessionID& sessionID) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType)
    {
        crack(message, sessionID);
    }

    void onMessage(const FIX44::Logon& logon, const FIX::SessionID& sessionID)
    {
        bool valid_credentials = false;

        try
        {
            std::string username = logon.getField(553);
            std::string password = logon.getField(554);

            if (username == m_username && password == m_password)
            {
                valid_credentials = true;
            }
        }
        catch (...)
        {
        }

        if (valid_credentials == false)
        {
            FIX::Message logout;
            logout.getHeader().setField(FIX::MsgType(FIX::MsgType_Logon));
            logout.getHeader().setField(FIX::MsgType(FIX::MsgType_Logout));
            logout.setField(FIX::Text("Invalid Username or Password"));

            FIX::Session::sendToTarget(logout, sessionID);
            FIX::Session* session = FIX::Session::lookupSession(sessionID);
            if (session) session->disconnect();
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////
    // NEW ORDERS
    void onMessage(const FIX44::NewOrderSingle& message, const FIX::SessionID& sessionID)
    {
        // EXTRACT MESSAGE FIELDS
        FIX::Symbol symbol;
        FIX::Side side;
        FIX::OrdType order_type;
        FIX::OrderQty orig_qty;
        FIX::Price price;
        FIX::ClOrdID clordid;
        FIX::Account account;

        message.get(order_type);
        message.get(symbol);
        message.get(side);
        message.get(orig_qty);
        message.get(price);
        message.get(clordid);

        if (symbol == m_fill_instrument)
        {
            // Fill
            double requested_qty = orig_qty.getValue();
            double filled_qty = 0;

            for (std::size_t i = 0; i < requested_qty; i++)
            {
                filled_qty++;

                FIX::OrdStatus status;
                FIX::ExecType exec_type;

                if (filled_qty == requested_qty)
                {
                    status = FIX::OrdStatus_FILLED;
                    exec_type = FIX::ExecType_FILL;
                }
                else
                {
                    status = FIX::OrdStatus_PARTIALLY_FILLED;
                    exec_type = FIX::ExecType_PARTIAL_FILL;
                }

                FIX44::ExecutionReport exec_report = FIX44::ExecutionReport
                (FIX::OrderID(generate_order_id()),
                    FIX::ExecID(generate_exec_id()),
                    exec_type,
                    FIX::OrdStatus(status),
                    side,
                    FIX::LeavesQty(requested_qty - filled_qty),
                    FIX::CumQty(filled_qty),
                    0
                    );

                exec_report.set(clordid);
                exec_report.set(symbol);
                exec_report.set(orig_qty);
                exec_report.set(FIX::LastQty(1));
                exec_report.set(FIX::LastPx(price));
                exec_report.set(FIX::AvgPx(price));

                add_test_repeating_groups(exec_report);

                FIX::Session::sendToTarget(exec_report, sessionID);
            }
        }
        else if (symbol == m_reject_instrument)
        {
            // Reject
            FIX44::ExecutionReport exec_report;

            exec_report.set(FIX::ExecType(FIX::ExecType_REJECTED));
            exec_report.set(FIX::OrdStatus(FIX::OrdStatus_REJECTED));
            exec_report.set(clordid);

            add_test_repeating_groups(exec_report);

            FIX::Session::sendToTarget(exec_report, sessionID);
        }
        else
        {
            // Ack
            FIX44::ExecutionReport exec_report = FIX44::ExecutionReport
            (FIX::OrderID(generate_order_id()),
                FIX::ExecID(generate_exec_id()),
                FIX::ExecType(FIX::ExecType_NEW),
                FIX::OrdStatus(FIX::OrdStatus_NEW),
                side,
                FIX::LeavesQty(orig_qty),
                FIX::CumQty(0),
                0);

            exec_report.set(clordid);
            exec_report.set(symbol);
            exec_report.set(orig_qty);

            add_test_repeating_groups(exec_report);

            FIX::Session::sendToTarget(exec_report, sessionID);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////
    // CANCEL ORDERS
    void onMessage(const FIX44::OrderCancelRequest& message, const FIX::SessionID& sessionID)
    {
        FIX::Symbol symbol;
        message.get(symbol);

        FIX::ClOrdID clordid;
        message.get(clordid);

        FIX::OrigClOrdID orig_clordid;
        message.get(orig_clordid);

        if (symbol != m_reject_instrument)
        {
            FIX44::ExecutionReport exec_report;

            exec_report.set(FIX::ExecType(FIX::ExecType_CANCELED));
            exec_report.set(FIX::OrdStatus(FIX::OrdStatus_CANCELED));
            exec_report.set(clordid);
            exec_report.set(orig_clordid);

            FIX::Session::sendToTarget(exec_report, sessionID);
        }
        else
        {
            // reject
            FIX44::OrderCancelReject reject;

            reject.set(clordid);
            reject.set(orig_clordid);

            FIX::Session::sendToTarget(reject, sessionID);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////
    // REPLACE ORDERS
    void onMessage(const FIX44::OrderCancelReplaceRequest& message, const FIX::SessionID& sessionID)
    {
        // EXTRACT MESSAGE FIELDS
        FIX::Symbol symbol;
        FIX::Side side;
        FIX::OrdType order_type;
        FIX::OrderQty orig_qty;
        FIX::Price price;
        FIX::ClOrdID clordid;
        FIX::Account account;
        FIX::OrigClOrdID orig_clordid;

        message.get(order_type);
        message.get(symbol);
        message.get(side);
        message.get(orig_qty);
        message.get(price);
        message.get(clordid);
        message.get(orig_clordid);

        auto send_replace_ack = [&]()
            {
                FIX44::ExecutionReport exec_report = FIX44::ExecutionReport
                (FIX::OrderID(generate_order_id()),
                    FIX::ExecID(generate_exec_id()),
                    FIX::ExecType(FIX::ExecType_REPLACED),
                    FIX::OrdStatus(FIX::OrdStatus_REPLACED),
                    side,
                    FIX::LeavesQty(orig_qty),
                    FIX::CumQty(0),
                    0);

                exec_report.set(clordid);
                exec_report.set(symbol);
                exec_report.set(orig_qty);
                exec_report.set(orig_clordid);

                FIX::Session::sendToTarget(exec_report, sessionID);
            };

        if (symbol != m_reject_instrument)
        {
            send_replace_ack();
        }
        else
        {
            // reject
            FIX44::OrderCancelReject reject;

            reject.set(clordid);
            reject.set(orig_clordid);

            FIX::Session::sendToTarget(reject, sessionID);
        }
    }

private:
    int m_order_id = 0;
    int m_exec_id = 0;

    std::string m_username;
    std::string m_password;

    std::string m_fill_instrument;
    std::string m_reject_instrument;

    std::string generate_order_id()
    {
        return std::to_string(++m_order_id);
    }

    std::string generate_exec_id()
    {
        return std::to_string(++m_exec_id);
    }

    void add_test_repeating_groups(FIX44::ExecutionReport& execReport)
    {
        FIX44::ExecutionReport::NoPartyIDs partyGroup1;
        partyGroup1.set(FIX::PartyID("PARTY1"));
        partyGroup1.set(FIX::PartyIDSource('D'));
        partyGroup1.set(FIX::PartyRole(1));
        execReport.addGroup(partyGroup1);

        FIX44::ExecutionReport::NoPartyIDs partyGroup2;
        partyGroup2.set(FIX::PartyID("PARTY2"));
        partyGroup2.set(FIX::PartyIDSource('D'));
        partyGroup2.set(FIX::PartyRole(3));
        execReport.addGroup(partyGroup2);
    }
};

#endif