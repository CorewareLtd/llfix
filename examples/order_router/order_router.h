#ifndef _ORDER_ROUTER_H_
#define _ORDER_ROUTER_H_

#include <cstdint>
#include <string>
#include <memory>
#include <thread>
#include <atomic>

#include <vector>
#include <unordered_map>

#include <llfix/incoming_fix_message.h>
#include <llfix/outgoing_fix_message.h>

#include <llfix/fix_server.h>
#include <llfix/core/utilities/tcp_reactor.h>

#include <llfix/fix_client.h>
#include <llfix/core/utilities/tcp_connector.h>

#include <llfix/core/utilities/spsc_bounded_queue.h>

struct RouteTagValuePair
{
    uint32_t tag = 0;
    std::string value;
};

struct RouteMessage
{
    std::string message_type;
    std::vector<RouteTagValuePair> body;
};

using MessageQueue = llfix::SPSCBoundedQueue<RouteMessage*>;

class OrderRouter : public llfix::FixServer<llfix::TcpReactor<>>
{
    public:
        bool init_acceptor(const std::string& config_file, const std::string& instance_name);
        void add_mapping(const std::string& inbound_session_name, const std::string& outbound_session_name);

        bool add_inbound_session(const std::string& config_file, const std::string& session_name);
        bool add_outbound_session(const std::string& client_config_file, const std::string& client_name, const std::string& session_config_file, const std::string& client_session_name);

        bool start();
        void shutdown();

        virtual void on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;
        virtual void on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override;
    private:
        std::unordered_map<std::string, std::string> m_mappings;
        std::atomic<bool> m_is_stopping = false;
        std::unique_ptr<std::thread> m_clients_thread;
        std::unordered_map<std::string, std::unique_ptr<llfix::FixClient<llfix::TCPConnector>>> m_clients;
        std::unordered_map<std::string, std::unique_ptr<MessageQueue>> m_messages_queues;

        void process_clients();
        void copy_message_contents(const llfix::IncomingFixMessage* source_message, RouteMessage* target_message);
        void copy_message_contents(RouteMessage* source_message, llfix::OutgoingFixMessage* target_message);
};

#endif