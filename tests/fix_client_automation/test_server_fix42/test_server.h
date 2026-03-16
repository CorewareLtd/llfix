#ifndef _TEST_SERVER_H_
#define _TEST_SERVER_H_

#include <cstddef>
#include <string>

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"

#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/NewOrderSingle.h"
#include "quickfix/fix42/OrderCancelRequest.h"
#include "quickfix/fix42/OrderCancelReplaceRequest.h"
#include "quickfix/fix42/OrderCancelReject.h"
#include "quickfix/fix42/Logon.h"

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

    /////////////////////////////////////////////////////////////////////////////////////////////////
    // NEW ORDERS
    void onMessage(const FIX42::NewOrderSingle& message, const FIX::SessionID& sessionID)
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

                FIX42::ExecutionReport exec_report = FIX42::ExecutionReport
                (FIX::OrderID(generate_order_id()),
                    FIX::ExecID(generate_exec_id()),
                    FIX::ExecTransType('0'),
                    exec_type,
                    FIX::OrdStatus(status),
                    symbol,
                    side,
                    FIX::LeavesQty(requested_qty - filled_qty),
                    FIX::CumQty(filled_qty),
                    0
                    );

                exec_report.set(clordid);
                exec_report.set(symbol);
                exec_report.set(orig_qty);
                exec_report.setField(32, "1");
                exec_report.set(FIX::LastPx(price));
                exec_report.set(FIX::AvgPx(price));

                exec_report.setField(17, "0");
                exec_report.setField(37, "0");
                exec_report.setField(20, "0");
                exec_report.setField(6, "0");

                FIX::Session::sendToTarget(exec_report, sessionID);
            }
        }
        else if (symbol == m_reject_instrument)
        {
            // Reject
            FIX42::ExecutionReport exec_report;

            exec_report.set(FIX::ExecType(FIX::ExecType_REJECTED));
            exec_report.set(FIX::OrdStatus(FIX::OrdStatus_REJECTED));
            exec_report.set(clordid);

            exec_report.setField(17, "0");
            exec_report.setField(37, "0");
            exec_report.setField(20, "0");
            exec_report.setField(6, "0");
            exec_report.setField(54, "1");
            exec_report.setField(151, "0");
            exec_report.setField(14, "0");
            exec_report.setField(55, symbol.getString());

            FIX::Session::sendToTarget(exec_report, sessionID);
        }
        else
        {
            // Ack
            FIX42::ExecutionReport exec_report = FIX42::ExecutionReport
            (FIX::OrderID(generate_order_id()),
                FIX::ExecID(generate_exec_id()),
                FIX::ExecTransType('0'),
                FIX::ExecType(FIX::ExecType_NEW),
                FIX::OrdStatus(FIX::OrdStatus_NEW),
                symbol,
                side,
                FIX::LeavesQty(orig_qty),
                FIX::CumQty(0),
                0);

            exec_report.set(clordid);
            exec_report.set(symbol);
            exec_report.set(orig_qty);

            exec_report.setField(17, "0");
            exec_report.setField(37, "0");
            exec_report.setField(20, "0");
            exec_report.setField(6, "0");
            exec_report.setField(54, "1");
            exec_report.setField(14, "0");
            exec_report.setField(151, orig_qty.getString());
            exec_report.setField(55, symbol.getString());

            FIX::Session::sendToTarget(exec_report, sessionID);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////
    // CANCEL ORDERS
    void onMessage(const FIX42::OrderCancelRequest& message, const FIX::SessionID& sessionID)
    {
        FIX::Symbol symbol;
        message.get(symbol);

        FIX::ClOrdID clordid;
        message.get(clordid);

        FIX::OrigClOrdID orig_clordid;
        message.get(orig_clordid);

        if (symbol != m_reject_instrument)
        {
            FIX42::ExecutionReport exec_report;

            exec_report.set(FIX::ExecType(FIX::ExecType_CANCELED));
            exec_report.set(FIX::OrdStatus(FIX::OrdStatus_CANCELED));
            exec_report.set(clordid);
            exec_report.set(orig_clordid);

            exec_report.setField(17, "0");
            exec_report.setField(37, "0");
            exec_report.setField(20, "0");
            exec_report.setField(6, "0");
            exec_report.setField(54, "1");
            exec_report.setField(14, "0");
            exec_report.setField(151, "0");
            exec_report.setField(55, symbol.getString());

            FIX::Session::sendToTarget(exec_report, sessionID);
        }
        else
        {
            // reject
            FIX42::OrderCancelReject reject;

            reject.set(clordid);
            reject.set(orig_clordid);

            reject.setField(39, "4");
            reject.setField(37, "0");
            reject.setField(434, "1");

            FIX::Session::sendToTarget(reject, sessionID);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////
    // REPLACE ORDERS
    void onMessage(const FIX42::OrderCancelReplaceRequest& message, const FIX::SessionID& sessionID)
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
                FIX42::ExecutionReport exec_report = FIX42::ExecutionReport
                (FIX::OrderID(generate_order_id()),
                    FIX::ExecID(generate_exec_id()),
                    FIX::ExecTransType('0'),
                    FIX::ExecType(FIX::ExecType_REPLACED),
                    FIX::OrdStatus(FIX::OrdStatus_REPLACED),
                    symbol,
                    side,
                    FIX::LeavesQty(orig_qty),
                    FIX::CumQty(0),
                    0);

                exec_report.set(clordid);
                exec_report.set(symbol);
                exec_report.set(orig_qty);
                exec_report.set(orig_clordid);

                exec_report.setField(17, "0");
                exec_report.setField(37, "0");
                exec_report.setField(20, "0");
                exec_report.setField(6, "0");
                exec_report.setField(54, "1");
                exec_report.setField(14, "0");
                exec_report.setField(151, orig_qty.getString());
                exec_report.setField(55, symbol.getString());

                FIX::Session::sendToTarget(exec_report, sessionID);
            };

        if (symbol != m_reject_instrument)
        {
            send_replace_ack();
        }
        else
        {
            // reject
            FIX42::OrderCancelReject reject;

            reject.set(clordid);
            reject.set(orig_clordid);

            reject.setField(39, "4");
            reject.setField(37, "0");
            reject.setField(434, "1");

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
};

#endif