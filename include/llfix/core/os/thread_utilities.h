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
    Provides :

                static unsigned int get_number_of_logical_cores()
                static unsigned int get_number_of_physical_cores()
                static bool is_hyper_threading()

                static inline void yield()
                static inline void sleep_in_nanoseconds(uint64_t nanoseconds)

                static int get_current_core_id()
                static unsigned long get_current_thread_id()

                static int pin_calling_thread_to_cpu_core(int core_id)
                static void set_thread_name(unsigned long thread_id, const std::string_view name)
*/
#ifdef __linux__ // VOLTRON_EXCLUDE
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#elif _WIN32 // VOLTRON_EXCLUDE
#include <windows.h>
#include <chrono>
#include <thread>
#endif // VOLTRON_EXCLUDE

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <string_view>

/*
    Currently this module is not hybrid-architecture-aware
    Ex: P-cores and E-cores starting from Alder Lake
    That means all methods assume that all CPU cores are identical
*/

namespace llfix
{

class ThreadUtilities
{
    public:

        static constexpr inline int MAX_THREAD_NAME_LENGTH = 15; // Limitation comes from Linux

        static unsigned int get_number_of_logical_cores()
        {
            unsigned int num_cores{0};
            #ifdef __linux__
            auto ret = sysconf(_SC_NPROCESSORS_ONLN);
            if(ret > 0)
                num_cores = static_cast<unsigned int>(ret);
            #elif _WIN32
            auto num_groups = GetActiveProcessorGroupCount();
            for (WORD i = 0; i < num_groups; ++i)
                num_cores += static_cast<unsigned int>(GetActiveProcessorCount(i));
            #endif
            return num_cores;
        }

        static unsigned int get_number_of_physical_cores()
        {
            unsigned int ret = get_number_of_logical_cores();
            if(is_hyper_threading() && ret>1)
            {
                ret = ret /2;
            }
            return ret;
        }

        static bool is_hyper_threading()
        {
            bool ret = false;

            #ifdef __linux__
            // Using syscalls to avoid dynamic memory allocation
            int file_descriptor = open("/sys/devices/system/cpu/smt/active", O_RDONLY);

            if (file_descriptor != -1)
            {
                char value;
                if (read(file_descriptor, &value, sizeof(value)) > 0)
                {
                    int smt_active = value - '0';
                    ret = (smt_active > 0);
                }

                close(file_descriptor);
            }
            #elif _WIN32
            DWORD buffer_size{ 0 };

            if (GetLogicalProcessorInformation(nullptr, &buffer_size) == FALSE)
            {
                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer_size == 0)
                    return true; // Assume hyper-threading is enabled if we can't get the processor information
            }

            char* buffer = static_cast<char*>(malloc(buffer_size));

            if (buffer == nullptr) return true; // Assume hyper-threading is enabled in case of memory allocation failure

            if (GetLogicalProcessorInformation(reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(buffer), &buffer_size) == FALSE)
            {
                free(buffer);
                return true; // Assume hyper-threading is enabled if we can't get the processor information
            }

            DWORD num_system_logical_processors = buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

            auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(buffer);

            for (DWORD i = 0; i < num_system_logical_processors; ++i)
            {
                auto& current = info[i];
                if (current.Relationship == RelationProcessorCore && (current.ProcessorCore.Flags & LTP_PC_SMT))
                {
                    ret = true; break;
                }
            }

            free(buffer);
            #endif
            return ret;
        }

        static inline void yield()
        {
            #ifdef __linux__
            sched_yield();
            #elif _WIN32
            SwitchToThread();
            #endif
        }

        static inline void sleep_in_nanoseconds(uint64_t nanoseconds)
        {
            #ifdef __linux__
            struct timespec ts;
            ts.tv_sec = static_cast<time_t>(nanoseconds / 1'000'000'000ULL);
            ts.tv_nsec = static_cast<long>(nanoseconds % 1'000'000'000ULL);
            nanosleep(&ts, nullptr);
            #elif _WIN32
            std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
            #endif
        }

        static int pin_calling_thread_to_cpu_core(int core_id)
        {
            int ret{ -1 };
            #ifdef __linux__
            if (core_id < 0 || core_id >= CPU_SETSIZE) return -1;
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);
            ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
            #elif _WIN32
            // Not multi processor group aware
            if (core_id < 0 || core_id >= static_cast<int>(sizeof(DWORD_PTR) * 8) ) return -1;

            const DWORD_PTR mask = (DWORD_PTR(1) << core_id);
            if (SetThreadAffinityMask(GetCurrentThread(), mask) != 0)
                ret = 0;

            #endif
            return ret;
        }

        static int get_current_core_id()
        {
            int current_core_id{ -1 };
            #ifdef __linux__
            current_core_id = ::sched_getcpu();
            #elif _WIN32
            current_core_id = ::GetCurrentProcessorNumber();
            #endif
            return current_core_id;
        }

        static unsigned long get_current_thread_id()
        {
            #ifdef __linux__
            return pthread_self();
            #elif _WIN32
            return ::GetCurrentThreadId();
            #endif
        }

        static void set_thread_name(unsigned long thread_id, const std::string_view name)
        {
            assert(name.length() > 0  && name.length() <= MAX_THREAD_NAME_LENGTH);

            #ifdef __linux__
            pthread_setname_np(thread_id, name.data());
            #elif _WIN32
            // As documented on MSDN
            // https://msdn.microsoft.com/en-us/library/xcb2z8hs(v=vs.120).aspx
            const DWORD MS_VC_EXCEPTION = 0x406D1388;

            #pragma pack(push,8)
            typedef struct tagTHREADNAME_INFO
            {
                DWORD dwType; // Must be 0x1000.
                LPCSTR szName; // Pointer to name (in user addr space).
                DWORD dwThreadID; // Thread ID (-1=caller thread).
                DWORD dwFlags; // Reserved for future use, must be zero.
            } THREADNAME_INFO;
            #pragma pack(pop)

            THREADNAME_INFO info;
            info.dwType = 0x1000;
            info.szName = name.data();
            info.dwThreadID = thread_id;
            info.dwFlags = 0;

            __try
            {
                RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            #endif
        }

    private:
};

} // namespace