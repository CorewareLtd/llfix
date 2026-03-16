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

#include <cstddef>
#include <new>

namespace llfix
{

#ifdef __linux__ // VOLTRON_EXCLUDE
#include <unistd.h>
#include <sys/select.h>
#elif _WIN32 // VOLTRON_EXCLUDE
#include <Ws2tcpip.h>
#endif // VOLTRON_EXCLUDE

#include "../compiler/unused.h"

#ifdef __linux__
using SelectSocketFDType = int;
#elif defined(_WIN32)
using SelectSocketFDType = SOCKET;
#endif

class Select
{
    public:

        Select() = default;

        ~Select()
        {
            if (m_query_set)
            {
                delete m_query_set;
                m_query_set = nullptr;
            }

            if (m_result_set)
            {
                delete m_result_set;
                m_result_set = nullptr;
            }
        }

        static constexpr bool polls_per_socket()
        {
            return true;
        }

        void clear_descriptors()
        {
            if(m_query_set)
                FD_ZERO(m_query_set);
        }

        bool initialise(long timeout_nanoseconds, std::size_t max_event_count=64)
        {
            LLFIX_UNUSED(max_event_count);

            if(m_query_set==nullptr)
            {
                m_query_set = new(std::nothrow) fd_set;

                if(m_query_set == nullptr)
                    return false;
            }

            if(m_result_set == nullptr)
            {
                m_result_set = new(std::nothrow) fd_set;

                if(m_result_set == nullptr)
                    return false;
            }

            FD_ZERO(m_query_set);
            FD_ZERO(m_result_set);

            // Timeout
            m_timeout.tv_sec = 0;
            m_timeout.tv_usec = 0;

            if(timeout_nanoseconds>0)
            {
                m_timeout.tv_sec = timeout_nanoseconds / 1'000'000'000;

                if(timeout_nanoseconds % 1'000'000'000 > 0)
                    m_timeout.tv_usec = (timeout_nanoseconds % 1'000'000'000) / 1000;
            }

            return true;
        }

        void add_descriptor(SelectSocketFDType fd)
        {
            if (fd > m_max_descriptor)
            {
                m_max_descriptor = fd;
            }

            FD_SET(fd, m_query_set);
        }

        void remove_descriptor(SelectSocketFDType fd)
        {
            if (FD_ISSET(fd, m_query_set))
            {
                FD_CLR(fd, m_query_set);
            }
        }

        int get_number_of_ready_descriptors()
        {
            *m_result_set = *m_query_set;
            #ifdef __linux__
            return ::select(m_max_descriptor + 1, m_result_set, nullptr, nullptr, &m_timeout);
            #elif _WIN32
            return ::select(0, m_result_set, nullptr, nullptr, &m_timeout);
            #endif
        }

        bool is_descriptor_ready(SelectSocketFDType fd)
        {
            bool ret{ false };

            ret = (FD_ISSET(fd, m_result_set)) ? true : false;

            return ret;
        }

        //////////////////////////////////////////////////////////////////////////////////////
        // THE FUNCTION BELOW ARE DEFINED TO COMPLY EPOLL LIKE ASYNC IO POLLERS
        // THEY EXIST MAINLY DUE TO OLDER GCCs WHICH HAS WEAK IF-CONSTEXPR SUPPORT
        int get_number_of_ready_events()
        {
            return -1;
        }

        bool is_valid_event(int index)
        {
            LLFIX_UNUSED(index);
            return false;
        }

        int get_ready_descriptor(int index)
        {
            LLFIX_UNUSED(index);
            return -1;
        }
        ///////////////////////////////////////////

    private :
        #ifdef __linux__
        SelectSocketFDType m_max_descriptor = -1;
        #elif _WIN32
        SelectSocketFDType m_max_descriptor = 0;
        #endif
        struct timeval m_timeout{0, 0};
        fd_set* m_query_set = nullptr;
        fd_set* m_result_set = nullptr;
};

} // namespace