/*
MIT License

Copyright (c) 2026 Coreware Limited

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "../core/utilities/logger.h"
#include "../electronic_trading/managed_instance/managed_instance.h"
#include "../fix_session.h"

namespace llfix
{

struct ManagementContext
{
    ManagementContext() = default;

    uint64_t application_start_timestamp = 0; // Nanoseconds since epoch
    std::string engine_version;
    std::string log_file_path;

    std::vector<ManagedInstance*> server_instances;
    std::vector<ManagedInstance*> client_instances;

    ManagedInstance* get_instance(const std::string& instance_name)
    {
        for (auto& server : server_instances)
        {
            if(server)
            {
                if (server->get_name() == instance_name)
                {
                    return server;
                }
            }
        }

        for (auto& client : client_instances)
        {
            if(client)
            {
                if (client->get_name() == instance_name)
                {
                    return client;
                }
            }
        }

        return nullptr;
    }

    bool has_instance(const std::string& instance_name)
    {
        return get_instance(instance_name) != nullptr;
    }

    FixSession* get_session(const std::string& instance_name, const std::string& session_name)
    {
        FixSession* ret{ nullptr };

        auto instance = get_instance(instance_name);

        if(instance)
        {
            ret = reinterpret_cast<FixSession*>(instance->get_session(session_name));
        }

        return ret;
    }
};

} // namespace