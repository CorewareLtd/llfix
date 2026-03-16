#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <cstdio>
#include <memory>

#include "order.h"
#include <llfix/core/utilities/object_cache.h>
#include <llfix/core/os/thread_utilities.h>

#include "../benchmark_utilities.h"

#include <fix8/f8includes.hpp>
#include <fix8/message.hpp>
#include <fix8/sessionwrapper.hpp>

#include "generated_code/Myfix_types.hpp"
#include "generated_code/Myfix_router.hpp"
#include "generated_code/Myfix_classes.hpp"

static constexpr std::size_t    ITERATION_COUNT_NEW_ORDER = 1'000'000;
static constexpr std::size_t    ANTISTALL_MOCK_ORDER_COUNT = 1000;
static constexpr int            CPU_CORE_INDEX_TO_PIN = 10;

class Fix8Router final : public FIX8::FIXT1100::Myfix_Router
{
    public:
        bool operator()(const FIX8::FIXT1100::ExecutionReport* message) const override
        {
            (void)(message);
            return true;
        }
};

class Fix8ClientSession final : public FIX8::Session
{
    public:
        explicit Fix8ClientSession(
            const FIX8::F8MetaCntx& ctx,
            const FIX8::SessionID& sid,
            FIX8::Persister* persist = nullptr,
            FIX8::Logger* logger = nullptr,
            FIX8::Logger* plogger = nullptr
        )
            : Session(ctx, sid, persist, logger, plogger)
        {
        }

        bool handle_application(const unsigned seqnum, const FIX8::Message*& msg) override
        {
            return enforce(seqnum, msg) || msg->process(m_router);
        }

        void state_change(const FIX8::States::SessionStates before, const FIX8::States::SessionStates after) override
        {
            // fprintf(stderr, "Fix8 session state change: %d -> %d\n", static_cast<int>(before), static_cast<int>(after));
            (void)(before);

            if (m_affinity_set == false && after == FIX8::States::st_continuous)
            {
                set_affinity(CPU_CORE_INDEX_TO_PIN);
                m_affinity_set = true;
            }
        }

    private:
        Fix8Router m_router;
        bool m_affinity_set = false;
};

class Client
{
    private:
        std::atomic<bool> m_start_benchmark = false;
        llfix::ObjectCache<Order> m_order_cache;

        const std::string m_symbol = "NOKIA.HE";
        const std::string m_clordid = "1"; // Fixed order id is not realistic but that is for benchmark fairness to avoid std::string construction

        FIX8::FIXT1100::OrderQty* m_order_qty = nullptr;
        FIX8::FIXT1100::Price* m_price = nullptr;
        FIX8::FIXT1100::TransactTime* m_transact_time = nullptr;
        FIX8::FIXT1100::ClOrdID* m_clordid_field = nullptr;
        FIX8::FIXT1100::Symbol* m_symbol_field = nullptr;
        FIX8::FIXT1100::OrdType* m_ord_type = nullptr;
        FIX8::FIXT1100::Side* m_side = nullptr;
        FIX8::FIXT1100::TimeInForce* m_time_in_force = nullptr;
        FIX8::FIXT1100::NoPartyIDs* m_no_party_ids = nullptr;
        FIX8::FIXT1100::PartyID* m_party1_id = nullptr;
        FIX8::FIXT1100::PartyIDSource* m_party1_id_source = nullptr;
        FIX8::FIXT1100::PartyRole* m_party1_role = nullptr;
        FIX8::FIXT1100::PartyID* m_party2_id = nullptr;
        FIX8::FIXT1100::PartyIDSource* m_party2_id_source = nullptr;
        FIX8::FIXT1100::PartyRole* m_party2_role = nullptr;

        unsigned long long m_cpu_frequency = 0;
        Stopwatch<StopwatchType::STOPWATCH_WITH_RDTSCP> m_stopwatch;
        Statistics<> m_stats;
        std::unique_ptr<FIX8::ClientSession<Fix8ClientSession>> m_client_session;
        Fix8ClientSession* m_session = nullptr;
        FIX8::FIXT1100::NewOrderSingle* m_reused_new_order = nullptr;

    public:
        void start()
        {
            if (m_client_session)
            {
                return;
            }

            try
            {
                #ifdef MEMORY_PERSISTENT
                fprintf(stdout, "FIX8 MODE : MEMORY PERSISTENT\n");
                m_client_session.reset(new FIX8::ClientSession<Fix8ClientSession>(FIX8::FIXT1100::ctx(), "client_memory_persistent.xml", "CLIENT"));
                #else
                fprintf(stdout, "FIX8 MODE : FILE PERSISTENT\n");
                m_client_session.reset(new FIX8::ClientSession<Fix8ClientSession>(FIX8::FIXT1100::ctx(), "client_file_persistent.xml", "CLIENT"));
                #endif
            }
            catch (const std::exception& ex)
            {
                fprintf(stderr, "Failed to load Fix8 config: %s\n", ex.what());
                return;
            }

            m_session = m_client_session->session_ptr();

            if (m_session == nullptr)
            {
                fprintf(stderr, "Failed to create Fix8 session\n");
                return;
            }

            const auto& davi_field = m_session->get_login_parameters()._davi;
            m_client_session->start(false, 0, 0, davi_field.get());
        }

        void stop()
        {
            if (m_session)
            {
                m_session->stop();
            }

            m_client_session.reset();
            m_session = nullptr;

            delete m_reused_new_order;
            m_reused_new_order = nullptr;
        }

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
                if (m_session && FIX8::States::is_established(m_session->get_session_state()))
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
            if(build_cached_order()==false)
            {
                fprintf(stderr, "Could not build the cached new order\n");
                return;
            }

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

        bool build_cached_order()
        {
            try
            {
                m_reused_new_order = new FIX8::FIXT1100::NewOrderSingle;

                m_order_qty = new FIX8::FIXT1100::OrderQty(0.0);
                m_price = new FIX8::FIXT1100::Price(0.0);
                m_transact_time = new FIX8::FIXT1100::TransactTime;

                m_clordid_field = new FIX8::FIXT1100::ClOrdID("");
                m_symbol_field = new FIX8::FIXT1100::Symbol(m_symbol);
                m_ord_type = new FIX8::FIXT1100::OrdType(FIX8::FIXT1100::OrdType_LIMIT);
                m_side = new FIX8::FIXT1100::Side(FIX8::FIXT1100::Side_BUY);
                m_time_in_force = new FIX8::FIXT1100::TimeInForce(FIX8::FIXT1100::TimeInForce_DAY);

                *m_reused_new_order << m_transact_time
                << m_order_qty
                << m_price
                << m_clordid_field
                << m_symbol_field
                << m_ord_type
                << m_side
                << m_time_in_force;

                m_no_party_ids = new FIX8::FIXT1100::NoPartyIDs(2);
                *m_reused_new_order << m_no_party_ids;

                FIX8::GroupBase* parties = m_reused_new_order->find_add_group<FIX8::FIXT1100::NewOrderSingle::NoPartyIDs>(nullptr);

                if (parties)
                {
                    parties->clear(true);
                    FIX8::MessageBase* party1 = parties->create_group(false);
                    m_party1_id = new FIX8::FIXT1100::PartyID("PARTY1");
                    m_party1_id_source = new FIX8::FIXT1100::PartyIDSource('D');
                    m_party1_role = new FIX8::FIXT1100::PartyRole(1);
                    *party1 << m_party1_id
                            << m_party1_id_source
                            << m_party1_role;
                    *parties << party1;

                    FIX8::MessageBase* party2 = parties->create_group(false);
                    m_party2_id = new FIX8::FIXT1100::PartyID("PARTY2");
                    m_party2_id_source = new FIX8::FIXT1100::PartyIDSource('D');
                    m_party2_role = new FIX8::FIXT1100::PartyRole(3);
                    *party2 << m_party2_id
                            << m_party2_id_source
                            << m_party2_role;
                    *parties << party2;
                }
            }
            catch(...)
            {
                return false;
            }
            return true;
        }

        void send_new_order(Order* order)
        {
            // fprintf(stderr , "cpu core : %d\n", llfix::ThreadUtilities::get_current_core_id());

            m_reused_new_order->setup_reuse();

            m_transact_time->set(FIX8::Tickval(true));
            m_order_qty->set(static_cast<double>(order->get_remaining_qty()));
            m_price->set(static_cast<double>(order->get_price()));

            m_clordid_field->set(m_clordid);
            m_symbol_field->set(m_symbol);
            m_ord_type->set(FIX8::FIXT1100::OrdType_LIMIT);
            m_side->set(FIX8::FIXT1100::Side_BUY);
            m_time_in_force->set(FIX8::FIXT1100::TimeInForce_DAY);
            m_no_party_ids->set(2);
            m_party1_id->set("PARTY1");
            m_party1_id_source->set('D');
            m_party1_role->set(1);
            m_party2_id->set("PARTY2");
            m_party2_id_source->set('D');
            m_party2_role->set(3);

            m_session->send(m_reused_new_order, false);
        }
};