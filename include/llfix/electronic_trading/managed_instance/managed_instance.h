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

#include <cstdint>
#include <vector>
#include <string>

#include "modifying_admin_command.h"
#include "managed_instance_session.h"

namespace llfix
{

class ManagedInstance
{
    public:
        virtual ~ManagedInstance() = default;
        virtual std::string get_name() const = 0;
        virtual std::string get_settings_as_string(const std::string& delimiter) = 0;
        virtual void push_admin_command(const std::string& session_name, ModifyingAdminCommandType type, uint32_t arg = 0) = 0;
        virtual void get_session_names(std::vector<std::string>& target) = 0;
        virtual bool is_instance_ha_primary() const = 0;
        virtual ManagedInstanceSession* get_session(const std::string& session_name="") = 0;
};

}