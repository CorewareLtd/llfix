#ifndef _EXCHANGE_SIMULATOR_H_
#define _EXCHANGE_SIMULATOR_H_

#include <cstdint>
#include <string>
#include <unordered_map>

#include <llfix/fix_server.h>
#include <llfix/core/utilities/tcp_reactor.h>

enum class SimulatorMode
{
    NO_FILLS,
    FILL_NEW_ORDER_ONE_BY_ONE,
    FILL_NEW_ORDER_AT_ONCE,
    FILL_REPLACE_ORDER_ONE_BY_ONE,
    FILL_REPLACE_ORDER_AT_ONCE,
    REJECT_ALL_REQUESTS
};

struct SimulatorExecReport
{
    std::string clorid;
    std::string orig_clordid;
    char exec_type='0';
    uint32_t ord_status = 0;

    int fill_qty = -1;
    int remaining_qty = -1;
    int cum_qty = -1;
    int price = -1;

    char side = (char)(0);
    std::string symbol;

    bool include_repeating_group = true;
};

struct SimulatorStats
{
    uint32_t new_order_count = 0;
    uint32_t replace_order_count = 0;
    uint32_t cancel_order_count = 0;
    uint32_t ack_count = 0;
    uint32_t part_fill_count = 0;
    uint32_t full_fill_count = 0;
    uint32_t reject_count = 0;
};

// For multithreaded server, specialise with  llfix::TcpReactorScalable<>    instead of llfix::TcpReactor<>
// For SSL,                  specialise with  llfix::TcpReactorScalableSSL<> instead of llfix::TcpReactor<>
class ExchangeSimulator : public llfix::FixServer<llfix::TcpReactor<>>
{
    private:
        SimulatorMode m_mode = SimulatorMode::FILL_NEW_ORDER_ONE_BY_ONE;
        uint32_t m_execution_id = 0;
        std::unordered_map<std::string, SimulatorStats> m_stats; // Per session
    public:

        virtual void on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;
        virtual bool authenticate_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;
        virtual void on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;
        virtual void on_replace_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;
        virtual void on_cancel_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;

        void send_execution_report(llfix::FixSession* session, const SimulatorExecReport& contents);

        virtual void on_application_level_reject(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;
        virtual void on_session_level_reject(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;

        // 35=3 & 35=j
        void on_application_or_session_level_reject(const llfix::IncomingFixMessage* reject_message);

        SimulatorMode mode() const;
        void set_mode(SimulatorMode mode);

        std::string get_all_sessions_display_text();
        void init_stats_per_session();
        std::string get_all_sessions_stats_display_text();
};

#endif