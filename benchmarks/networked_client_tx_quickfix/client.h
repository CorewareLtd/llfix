#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <cstdio>

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Session.h"
#include "quickfix/fix50/NewOrderSingle.h"

#include "order.h"
#include <llfix/core/utilities/object_cache.h>
#include <llfix/core/os/thread_utilities.h>

#include "../benchmark_utilities.h"

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

static constexpr std::size_t    ITERATION_COUNT_NEW_ORDER = 1'000'000;
static constexpr std::size_t    ANTISTALL_MOCK_ORDER_COUNT = 1000;

class Client : public FIX::Application, public FIX::MessageCracker
{
    private:
        std::atomic<bool> m_start_benchmark = false;
        FIX::SessionID m_sessionID;
        std::atomic<bool> m_logged_on = false;
        llfix::ObjectCache<Order> m_order_cache;

        const std::string m_symbol = "NOKIA.HE";
        const std::string m_clordid = "1"; // Fixed order id is not realistic but that is for benchmark fairness to avoid std::string construction

        unsigned long long m_cpu_frequency = 0;
        Stopwatch<StopwatchType::STOPWATCH_WITH_RDTSCP> m_stopwatch;
        Statistics<> m_stats;
    public:

        void set_cpu_frequency(unsigned long long f) { m_cpu_frequency = f; }
        void set_start_benchmark(bool b) { m_start_benchmark = b; }
        Statistics<>& get_stats() { return m_stats; }

        void run()
        {
            if (m_order_cache.create(2048) == false)
            {
                fprintf(stderr, "Could not create order_cache\n");
                return;
            }

            while (true)
            {
                if (m_logged_on.load() == true)
                {
                    break;
                }
            }

            while (true)
            {
                if (m_start_benchmark.load() == true)
                {
                    break;
                }
            }

            ////////////////////////////////////////////////////////
            fprintf(stdout, "Antistall mock order count : %zu\n", ANTISTALL_MOCK_ORDER_COUNT);

            for (std::size_t i = 0; i < ITERATION_COUNT_NEW_ORDER + ANTISTALL_MOCK_ORDER_COUNT; i++)
            {
                m_stopwatch.start();

                auto new_order = m_order_cache.allocate();
                new_order->set_symbol(m_symbol);
                new_order->set_remaining_qty(10);
                new_order->set_price(10000);

                send_new_order(new_order);

                m_stopwatch.stop();

                if (i >= ANTISTALL_MOCK_ORDER_COUNT)
                {
                    m_stats.add_sample(static_cast<double>(m_stopwatch.get_elapsed_nanoseconds(m_cpu_frequency)));
                }
            }
        }

        void send_new_order(Order* order)
        {
            // fprintf(stderr , "cpu core : %d\n", llfix::ThreadUtilities::get_current_core_id());

            FIX50::NewOrderSingle newOrder(
                FIX::ClOrdID(m_clordid),
                FIX::Side(FIX::Side_BUY),
                FIX::TransactTime(3),
                FIX::OrdType(FIX::OrdType_LIMIT)
            );

            newOrder.set(FIX::Symbol(m_symbol));
            newOrder.set(FIX::OrderQty(order->get_remaining_qty()));
            newOrder.set(FIX::Price(static_cast<double>(order->get_price())));
            newOrder.set(FIX::TimeInForce(FIX::TimeInForce_DAY));

            // REPEATING GROUP
            FIX50::NewOrderSingle::NoPartyIDs partyGroup1;
            partyGroup1.set(FIX::PartyID("PARTY1"));
            partyGroup1.set(FIX::PartyIDSource('D'));
            partyGroup1.set(FIX::PartyRole(1));
            newOrder.addGroup(partyGroup1);

            FIX50::NewOrderSingle::NoPartyIDs partyGroup2;
            partyGroup2.set(FIX::PartyID("PARTY2"));
            partyGroup2.set(FIX::PartyIDSource('D'));
            partyGroup2.set(FIX::PartyRole(3));
            newOrder.addGroup(partyGroup2);

            FIX::Session::sendToTarget(newOrder, m_sessionID);
        }

    private:
        void onMessage(const FIX50::ExecutionReport& report, const FIX::SessionID& sessionID)
        {
            // We need to override otherwise Quickfix rejects incoming execution reports
        }

        void onCreate(const FIX::SessionID&) {}
        void toAdmin(FIX::Message&, const FIX::SessionID&) {}
        void fromAdmin(const FIX::Message&, const FIX::SessionID&) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) {}

        void onLogon(const FIX::SessionID& sessionID)
        {
            m_sessionID = sessionID;
            m_logged_on = true;
        }

        void onLogout(const FIX::SessionID& sessionID)
        {
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
};