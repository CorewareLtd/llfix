#include <llfix/common.h>

#include "order_router.h"

#include <string_view>
#include <sstream>

#include <llfix/core/compiler/unused.h>
#include <llfix/fix_protocol_version.h>
#include <llfix/core/utilities/logger.h>
#include <llfix/core/os/console.h>


void OrderRouter::on_logon_request(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    LLFIX_UNUSED(session);
    llfix::Console::print_colour(llfix::ConsoleColour::FG_BLUE, "Incoming logon req : " + message->to_string() + "\n");
}

void OrderRouter::add_mapping(const std::string& inbound_session_name, const std::string& outbound_session_name)
{
    m_mappings[inbound_session_name] = outbound_session_name;
}

bool OrderRouter::init_acceptor(const std::string& config_file, const std::string& instance_name)
{
    return create(instance_name, config_file);
}

bool OrderRouter::add_inbound_session(const std::string& config_file, const std::string& session_name)
{
    return add_session(config_file, session_name);
}

bool OrderRouter::add_outbound_session(const std::string& client_config_file, const std::string& client_name, const std::string& session_config_file, const std::string& client_session_name)
{
    std::unique_ptr<MessageQueue> current_message_queue(new MessageQueue());
    m_messages_queues[client_session_name] = std::move(current_message_queue);

    if (m_messages_queues[client_session_name]->create(1024) == false)
    {
        return false;
    }

    std::unique_ptr<llfix::FixClient<llfix::TCPConnector>> current_fix_client(new llfix::FixClient<llfix::TCPConnector>());

    m_clients[client_session_name] = std::move(current_fix_client);

    if (m_clients[client_session_name]->create(client_config_file, client_name, session_config_file, client_session_name) == false)
    {
        return false;
    }

    return true;
}

void OrderRouter::on_new_order(llfix::FixSession* session, const llfix::IncomingFixMessage* message)
{
    llfix::Console::print_colour(llfix::ConsoleColour::FG_GREEN, "New order : " + message->to_string() + "\n");

    auto target_session = m_mappings.find(session->get_name());

    if (target_session != m_mappings.end())
    {
        RouteMessage* route_message = new RouteMessage();

        if (route_message)
        {
            copy_message_contents(message, route_message);
            m_messages_queues[target_session->second]->push(route_message);
        }
    }
}

bool OrderRouter::start()
{
    if (FixServer::start() == false)
    {
        return false;
    }

    m_clients_thread.reset(new std::thread(&OrderRouter::process_clients, this));

    return true;
}

void OrderRouter::process_clients()
{
    while(true)
    {
        if(m_is_stopping.load() == true)
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

                        delete route_message;
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

void OrderRouter::shutdown()
{
    m_is_stopping = true;

    FixServer::shutdown();

    if (m_clients_thread && m_clients_thread->joinable())
    {
        m_clients_thread->join();
    }
}

void OrderRouter::copy_message_contents(const llfix::IncomingFixMessage* source_message, RouteMessage* target_message)
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

    copy_tag(11);
    copy_tag(40);
    copy_tag(55);
    copy_tag(54);
    copy_tag(44);
}

void OrderRouter::copy_message_contents(RouteMessage* source_message, llfix::OutgoingFixMessage* target_message)
{
    // MSG TYPE
    target_message->set_msg_type(source_message->message_type);

    // BODY TAGS
    for (const auto& tag_value_pair : source_message->body)
    {
        target_message->set_tag(tag_value_pair.tag, tag_value_pair.value);
    }
}