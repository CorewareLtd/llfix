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
/*
    Uses ( level triggered ) epoll on Linux and select on Windows
*/
#include <cstddef>
#include <mutex>
#include <new>
#include "../compiler/unused.h"

namespace llfix
{

#ifdef __linux__

#include <cassert>
#include <sys/epoll.h>
#include <unistd.h>

using EpollSocketFDType = int;

class Epoll
{
    public:
        Epoll()
        {
            m_epoll_descriptor = epoll_create1(0);
        }

        ~Epoll()
        {
            if (m_epoll_descriptor >= 0)
            {
                ::close(m_epoll_descriptor);
            }

            if (m_epoll_events)
            {
                delete[] m_epoll_events;
                m_epoll_events = nullptr;
            }
        }

        static constexpr bool polls_per_socket()
        {
            return false;
        }

        bool initialise(long timeout_nanoseconds, std::size_t max_event_count=64)
        {
            // Epoll events
            m_max_epoll_events = max_event_count;

            if(m_epoll_events)
            {
                delete[] m_epoll_events;
                m_epoll_events = nullptr;
            }

            m_epoll_events = new (std::nothrow) struct epoll_event[m_max_epoll_events];

            if(m_epoll_events == nullptr)
            {
                return false;
            }

            // Timeout
            m_epoll_timeout_milliseconds = 0;

            if(timeout_nanoseconds>0)
            {
                m_epoll_timeout_milliseconds = timeout_nanoseconds / 1'000'000;
            }

            return true;
        }

        void clear_descriptors()
        {
            if (m_epoll_descriptor >= 0)
            {
                ::close(m_epoll_descriptor);
            }

            m_epoll_descriptor = epoll_create1(0);
        }

        void add_descriptor(EpollSocketFDType fd)
        {
            const std::lock_guard<std::mutex> lock(m_descriptor_lock);

            struct epoll_event epoll_descriptor{};
            epoll_descriptor.data.fd = fd;

            epoll_descriptor.events = EPOLLIN;

            epoll_ctl(m_epoll_descriptor, EPOLL_CTL_ADD, fd, &epoll_descriptor);
        }

        void remove_descriptor(EpollSocketFDType fd)
        {
            const std::lock_guard<std::mutex> lock(m_descriptor_lock);

            struct epoll_event epoll_descriptor;
            epoll_descriptor.data.fd = fd;
            epoll_descriptor.events = EPOLLIN;

            epoll_ctl(m_epoll_descriptor, EPOLL_CTL_DEL, fd, &epoll_descriptor);
        }

        int get_number_of_ready_events()
        {
            int result{ -1 };
            result = epoll_wait(m_epoll_descriptor, m_epoll_events, m_max_epoll_events, m_epoll_timeout_milliseconds);
            return result;
        }

        bool is_valid_event(int index)
        {
            if (m_epoll_events[index].events & EPOLLIN)
            {
                return true;
            }

            return false;
        }

        int get_ready_descriptor(int index)
        {
            int ret{ -1 };
            ret = m_epoll_events[index].data.fd;
            return ret;
        }

        //////////////////////////////////////////////////////////////////////////////
        // COMMON INTERFACE AS GCC'S SUPPORT FOR IF-CONSTEXPR IS NOT AS GOOD AS MSVC
        // EVEN THOUGH THEY WON'T BE CALLED GCC STILL WANTS TO SEE THEM
        int get_number_of_ready_descriptors() { assert(1==0);return 0;}
        bool is_descriptor_ready(EpollSocketFDType fd) { LLFIX_UNUSED(fd);assert(1==0); return false;}
        //////////////////////////////////////////////////////////////////////////////

    private:
        EpollSocketFDType m_epoll_descriptor = -1;
        struct epoll_event* m_epoll_events = nullptr;
        std::size_t m_max_epoll_events = 64;
        int m_epoll_timeout_milliseconds = 0;
        std::mutex m_descriptor_lock;
};

#elif _WIN32

#include <Ws2tcpip.h>

using EpollSocketFDType = SOCKET;

// Since WSAPoll is broken, Windows implementation uses select to emulate epoll.
// By default it can only monitor 64 sockets. To increase that limit, you shall set FD_SETSIZE before winsock inclusion : https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
class Epoll
{
    public:
        Epoll() = default;

        ~Epoll()
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

        bool initialise(long timeout_nanoseconds, std::size_t max_event_count = 64)
        {
            LLFIX_UNUSED(max_event_count);

            if(m_query_set==nullptr)
            {
                m_query_set = new (std::nothrow) fd_set;

                if(m_query_set == nullptr)
                    return false;
            }

            if(m_result_set == nullptr)
            {
                m_result_set = new (std::nothrow) fd_set;

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

        void clear_descriptors()
        {
            if(m_query_set)
                FD_ZERO(m_query_set);
        }

        void add_descriptor(EpollSocketFDType fd)
        {
            const std::lock_guard<std::mutex> lock(m_descriptor_lock);

            if (fd > m_max_descriptor)
            {
                m_max_descriptor = fd;
            }

            FD_SET(fd, m_query_set);
        }

        void remove_descriptor(EpollSocketFDType fd)
        {
            const std::lock_guard<std::mutex> lock(m_descriptor_lock);

            if (FD_ISSET(fd, m_query_set))
            {
                FD_CLR(fd, m_query_set);
            }
        }

        int get_number_of_ready_descriptors()
        {
            *m_result_set = *m_query_set;
            return ::select(0, m_result_set, nullptr, nullptr, &m_timeout);
        }

        bool is_descriptor_ready(EpollSocketFDType fd)
        {
            bool ret{ false };

            ret = (FD_ISSET(fd, m_result_set)) ? true : false;

            return ret;
        }

    private:
        EpollSocketFDType m_max_descriptor = 0;
        struct timeval m_timeout{0, 0};
        fd_set* m_query_set = nullptr;
        fd_set* m_result_set = nullptr;
        std::mutex m_descriptor_lock;
};

#endif

} // namespace