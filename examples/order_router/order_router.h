#pragma once

#include <llfix/common.h>

#include <cstdint>
#include <string>
#include <memory>
#include <thread>
#include <atomic>

#include <string_view>
#include <vector>
#include <unordered_map>
#include <sstream>

#include <llfix/core/compiler/unused.h>
#include <llfix/core/os/console.h>
#include <llfix/core/utilities/object_cache.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/core/utilities/logger.h>

#include <llfix/fix_protocol_version.h>
#include <llfix/incoming_fix_message.h>
#include <llfix/outgoing_fix_message.h>

#include <llfix/core/utilities/spsc_bounded_queue.h>

#include "order_router_settings.h"

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

template <typename FixServerType, typename FixClientType>
class OrderRouter : public FixServerType
{
    public:

        static constexpr std::string_view ACCEPTOR_INSTANCE_NAME = "ROUTER_ACCEPTOR";

        bool initialise(const std::string& router_config_file, const std::string& acceptor_config_file)
        {
            if (!FixServerType::create(ACCEPTOR_INSTANCE_NAME.data(), acceptor_config_file))
            {
                return false;
            }

            if (!m_route_settings.load_from_config_file(router_config_file))
            {
                return false;
            }

            if (!m_route_settings.validate())
            {
                return false;
            }

            // Load mappings
            auto mapping_pairs = llfix::StringUtilities::split(m_route_settings.mappings, ',');

            for (const auto& mapping_pair : mapping_pairs)
            {
                auto tokens = llfix::StringUtilities::split(mapping_pair, '-');

                if (tokens.size() != 2)
                {
                    return false;
                }

                add_mapping(tokens[0], tokens[1]);

            }

            auto load_route_tags = [&](const std::string& tags_comma_separated, std::vector<uint32_t>& target)
                {
                    auto tags = llfix::StringUtilities::split(tags_comma_separated, ',');

                    try
                    {
                        for (const auto& tag : tags)
                        {
                            target.push_back(std::stoul(tag));
                        }
                    }
                    catch (...)
                    {
                        return false;
                    }

                    return true;
                };

            // Load new order route tags
            if (m_route_settings.new_order_route_tags.length() > 0)
            {
                if (!load_route_tags(m_route_settings.new_order_route_tags, m_new_order_route_tags))
                {
                    return false;
                }
            }

            // Load replace order route tags
            if (m_route_settings.replace_order_route_tags.length() > 0)
            {
                if (!load_route_tags(m_route_settings.replace_order_route_tags, m_replace_order_route_tags))
                {
                    return false;
                }
            }

            // Load cancel order route tags
            if (m_route_settings.cancel_order_route_tags.length() > 0)
            {
                if (!load_route_tags(m_route_settings.cancel_order_route_tags, m_cancel_order_route_tags))
                {
                    return false;
                }
            }

            return true;
        }

        bool add_inbound_session(const std::string& config_file, const std::string& session_name)
        {
            return FixServerType::add_session(config_file, session_name);
        }

        bool add_outbound_session(const std::string& client_config_file, const std::string& client_name, const std::string& session_config_file, const std::string& client_session_name)
        {
            std::unique_ptr<MessageQueue> current_message_queue(new MessageQueue());
            m_messages_queues[client_session_name] = std::move(current_message_queue);

            if (m_messages_queues[client_session_name]->create(1024) == false)
            {
                return false;
            }

            std::unique_ptr<FixClientType> current_fix_client(new FixClientType());

            m_clients[client_session_name] = std::move(current_fix_client);

            if (m_clients[client_session_name]->create(client_config_file, client_name, session_config_file, client_session_name) == false)
            {
                return false;
            }

            return true;
        }

        bool start()
        {
            if (m_route_message_cache.create(4096) == false)
            {
                return false;
            }
            if (FixServerType::start() == false)
            {
                return false;
            }

            m_clients_thread.reset(new std::thread(&OrderRouter::process_clients, this));

            return true;
        }

        void shutdown()
        {
            m_is_stopping = true;

            FixServerType::shutdown();

            if (m_clients_thread && m_clients_thread->joinable())
            {
                m_clients_thread->join();
            }
        }

        virtual void on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(session);
            llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Incoming logon req : " + message->to_string() + "\n");
        }

        virtual void on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_GREEN, "New order : " + message->to_string() + "\n");
            route_message(session, message, m_new_order_route_tags);
        }

        virtual void on_replace_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_GREEN, "Replace order : " + message->to_string() + "\n");
            route_message(session, message, m_replace_order_route_tags);
        }

        virtual void on_cancel_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message) override
        {
            llfix::Console::print_colour(llfix::ConsoleColour::FG_GREEN, "Cancel order : " + message->to_string() + "\n");
            route_message(session, message, m_cancel_order_route_tags);
        }

        std::string find_target_session(llfix::FixSession* session)
        {
            std::string ret;

            auto target_session = m_mappings.find(session->get_name());

            if (target_session != m_mappings.end())
            {
                ret = target_session->second;
            }

            return ret;
        }

    private:

        std::atomic<bool> m_is_stopping = false;
        std::unordered_map<std::string, std::unique_ptr<FixClientType>> m_clients;
        std::unique_ptr<std::thread> m_clients_thread;

        std::unordered_map<std::string, std::unique_ptr<MessageQueue>> m_messages_queues;
        llfix::ObjectCache<RouteMessage> m_route_message_cache;

        OrderRouterSettings m_route_settings;
        std::unordered_map<std::string, std::string> m_mappings;
        std::vector<uint32_t> m_new_order_route_tags;
        std::vector<uint32_t> m_replace_order_route_tags;
        std::vector<uint32_t> m_cancel_order_route_tags;

        void add_mapping(const std::string& inbound_session_name, const std::string& outbound_session_name)
        {
            m_mappings[inbound_session_name] = outbound_session_name;
        }

        void route_message(llfix::FixSession* session, const llfix::IncomingFixMessage* message, std::vector<uint32_t>& tags)
        {
            auto target_session_name = find_target_session(session);

            if (!target_session_name.empty())
            {
                RouteMessage* route_message = m_route_message_cache.allocate();

                if (route_message)
                {
                    copy_message_contents(message, route_message, tags);
                    m_messages_queues[target_session_name]->push(route_message);
                }
            }
        }

        void copy_message_contents(const llfix::IncomingFixMessage* source_message, RouteMessage* target_message, std::vector<uint32_t>& tags)
        {
            // MSG TYPE
            target_message->message_type = source_message->get_tag_value_as<std::string>(35);

            // EXPECTED BODY TAGS
            auto copy_tag = [&](uint32_t tag)
                {
                    if (source_message->has_tag(tag))
                    {
                        target_message->body.push_back({ tag, source_message->get_tag_value_as<std::string>(tag) });
                    }
                };

            for (const auto& tag : tags)
            {
                copy_tag(tag);
            }
        }

        void copy_message_contents(RouteMessage* source_message, llfix::OutgoingFixMessage* target_message)
        {
            // MSG TYPE
            target_message->set_msg_type(source_message->message_type);

            // BODY TAGS
            for (const auto& tag_value_pair : source_message->body)
            {
                target_message->set_tag(tag_value_pair.tag, tag_value_pair.value);
            }
        }

        void process_clients()
        {
            while (true)
            {
                if (m_is_stopping.load() == true)
                    break;

                for (auto& iter : m_clients)
                {
                    auto session = iter.second->get_session();
                    auto state = session->get_state();

                    if (state != llfix::SessionState::DISCONNECTED && state != llfix::SessionState::LOGGED_OUT)
                    {
                        iter.second->process();

                        RouteMessage* route_message = nullptr;
                        if (m_messages_queues[session->get_name()]->try_pop(&route_message))
                        {
                            if (route_message)
                            {
                                auto outgoing_message = iter.second->outgoing_message_instance();
                                copy_message_contents(route_message, outgoing_message);
                                iter.second->send_outgoing_message(outgoing_message);
                            }
                        }
                    }
                    else if (state == llfix::SessionState::DISCONNECTED || state == llfix::SessionState::LOGGED_OUT)
                    {
                        auto conn_success = iter.second->connect();
                        LLFIX_UNUSED(conn_success);
                    }
                }
            }
        }
};