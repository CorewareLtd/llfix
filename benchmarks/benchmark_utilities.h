/*

CLASSES:
        Stopwatch                                   based on RDTSCP ( or CPUID+RDTSC )

        ProcessorUtilities                          rdtscp rdtsc cpuid
                                                    get_current_cpu_frequency_hertz ( You need it to calc time from cpuid+RDTSC or RDTSCP tick counts from Stopwatch )
                                                    get_current_core_id
                                                    pin_calling_thread_to_cpu_core
                                                    cache_flush

        Statistics                                  Gives you min,max,average and percentiles : P50 P75 P90 P95 P99

        Console                                     print_colour

        LinuxInfo                                   Linux kernel version & CPU Isolation info

        RandomNumberGenerator                       Use it to get random integers

BENCHMARK MACROS & DO_NOT_OPTIMISE MACRO :

            BENCHMARK_BEGIN(iteration_count)
            // YOUR CODE WHICH WILL BE BENCHMARKED
            BENCHMARK_END()
            // THEN AFTER IT TO SEE THE RESULTS YOU DO :
            report.print("title");

            // ADDITIONAL THINGS YOU CAN DO BETWEEN BENCHMARK_START & BENCHMARK_END :

            DO_NOT_OPTIMISE(var);


            //  BENCHMARK REPORT OUTPUT ( IT WILL BE COLOURED ) :

                Current CPU frequency ( not min or max ) : 1797000000 Hz

                Title : system malloc
                Iteration count : 100
                Minimum time : 34 nanoseconds
                Maximum time : 137 nanoseconds
                Average time : 46.780000 nanoseconds
                P50 : 35.000000 nanoseconds
                P75 : 56.000000 nanoseconds
                P90 : 76.000000 nanoseconds
                P95 : 87.000000 nanoseconds
                P99 : 137.000000 nanoseconds


            By default it uses RDTSCP. If you work on a system without RDTSCP ,then  change the line below :

            from         #define STOPWATCH_TYPE StopwatchType::STOPWATCH_WITH_RDTSCP

            to           #define STOPWATCH_TYPE StopwatchType::STOPWATCH_WITH_CPUID_AND_RDTSC
*/
#ifndef _BENCHMARK_UTILITIES_H_
#define _BENCHMARK_UTILITIES_H_

#include <array>
#include <cstddef>
#include <vector>
#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <type_traits>
#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>
#include <cstdio>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <intrin.h>
#if _DEBUG
#pragma message("Warning: Building in Debug mode !!!")
#endif
#elif __linux__
#include <x86intrin.h>
#include <linux/version.h>
#include <pthread.h>
#include <sched.h>
#endif

///////////////////////////////////////////////////////////////////
// ProcessorUtilities
class ProcessorUtilities
{
    public:

        static void cpuid(int function, unsigned int* eax, unsigned int* ebx, unsigned int* ecx, unsigned int* edx)
        {
            #ifdef __linux__
            // Not using cpuid.h on Linux as that header doesn`t have include protection
            asm volatile(
                "cpuid"
                : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                : "a" (function), "c" (0)
            );
            #elif _WIN32
            int info[4];
            __cpuid(info, function);
            *eax = info[0];
            *ebx = info[1];
            *ecx = info[2];
            *edx = info[3];
            #endif
        }

        static void cache_flush(void* address)
        {
            _mm_clflush(address);
        }

        // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=rdtsc&ig_expand=5802
        static unsigned long long rdtsc()
        {
            return __rdtsc();
        }

        // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=rdtscp&ig_expand=5802,5803
        static unsigned long long rdtscp()
        {
            unsigned int model_specific_register_contents;
            return __rdtscp(&model_specific_register_contents);
        }

        static unsigned long long get_current_cpu_frequency_hertz()
        {
            unsigned long long ret{ 0 };
            #ifdef _WIN32
            DWORD data, data_size = sizeof(data);
            SHGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "~MHz", nullptr, &data, &data_size);
            ret = ((unsigned long long)data * (unsigned long long)(1000 * 1000));
            #elif __linux__
            std::ifstream cpuinfo("/proc/cpuinfo");
            std::string line;
            while (std::getline(cpuinfo, line))
            {
                if (line.find("cpu MHz") != std::string::npos)
                {
                    std::size_t colon_pos = line.find(':');

                    if (colon_pos != std::string::npos)
                    {
                        std::string freq_str = line.substr(colon_pos + 2);
                        ret = std::stol(freq_str) * 1000000; // Convert to hertz
                        break;
                    }
                }
            }
            cpuinfo.close();
            #endif
            return ret;
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
};

///////////////////////////////////////////////////////////////////
// Console
enum class ConsoleColour
{
    FG_DEFAULT,
    FG_RED,
    FG_GREEN,
    FG_BLUE,
    FG_YELLOW,
    COUNT
};

class Console
{
    public:
        struct ConsoleColourNode
        {
            ConsoleColour colour;
            int value;
        };

        static void print_colour(ConsoleColour foreground_colour, const std::string_view& buffer)
        {
            const std::lock_guard<std::mutex> lock(m_console_lock);
            auto fg_index = static_cast<std::underlying_type<ConsoleColour>::type>(foreground_colour);

            if (fg_index < 0 || fg_index >= static_cast<std::underlying_type<ConsoleColour>::type>(ConsoleColour::COUNT))
            {
                fg_index = static_cast<std::underlying_type<ConsoleColour>::type>(ConsoleColour::FG_DEFAULT);
            }

            auto foreground_colour_code = NATIVE_CONSOLE_COLOURS[fg_index].value;

            #ifdef _WIN32
            HANDLE handle_console = GetStdHandle(STD_OUTPUT_HANDLE);

            CONSOLE_SCREEN_BUFFER_INFO csbi;
            WORD old_attributes;

            if (GetConsoleScreenBufferInfo(handle_console, &csbi) != 0)
            {
                old_attributes = csbi.wAttributes;
            }
            else
            {
                old_attributes = 15; // default to white on black
            }
            SetConsoleTextAttribute(handle_console, foreground_colour_code | FOREGROUND_INTENSITY);
            fwrite(buffer.data(), 1, buffer.size(), stdout);
            SetConsoleTextAttribute(handle_console, old_attributes);
            #elif __linux__
            std::string ansi_colour_code = "\033[0;" + std::to_string(foreground_colour_code) + "m";
            fprintf(stdout, "%s", ansi_colour_code.c_str());
            fwrite(buffer.data(), 1, buffer.size(), stdout);
            fprintf(stdout, "\033[0m");
            #endif
        }

    private:
        static inline constexpr std::array<ConsoleColourNode, static_cast<std::underlying_type<ConsoleColour>::type>(ConsoleColour::COUNT)> NATIVE_CONSOLE_COLOURS =
        {
            //DO POD INITIALISATION
            {
                #ifdef __linux__
                // https://en.wikipedia.org/wiki/ANSI_escape_code#graphics
                ConsoleColour::FG_DEFAULT, 0,
                ConsoleColour::FG_RED, 31,
                ConsoleColour::FG_GREEN, 32,
                ConsoleColour::FG_BLUE, 34,
                ConsoleColour::FG_YELLOW, 33,
                #elif _WIN32
                ConsoleColour::FG_DEFAULT, 0,
                ConsoleColour::FG_RED, FOREGROUND_RED,
                ConsoleColour::FG_GREEN, FOREGROUND_GREEN,
                ConsoleColour::FG_BLUE, FOREGROUND_BLUE,
                ConsoleColour::FG_YELLOW, (FOREGROUND_RED | FOREGROUND_GREEN),
                #endif
            }
        };

        static inline std::mutex m_console_lock;
};

///////////////////////////////////////////////////////////////////
// LinuxInfo
#ifdef __linux__

class LinuxInfo
{
    public:

        static std::string get_linux_kernel_version()
        {
            std::string ret;

            unsigned int linux_version = LINUX_VERSION_CODE;
            unsigned int major = (linux_version >> 16) & 0xFF;
            unsigned int minor = (linux_version >> 8) & 0xFF;
            unsigned int revision = linux_version & 0xFF;

            ret = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(revision);

            return ret;
        }

        static std::string get_cpu_isolation_info()
        {
            std::string result;
            std::array<char, 128> buffer;

            FILE* pipe = popen("sudo cat /proc/cmdline | grep --color=auto isolcpus=", "r");
            if (!pipe)
            {
                throw std::runtime_error("popen() failed!");
            }

            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
            {
                result += buffer.data();
            }

            pclose(pipe);

            return result;
        }
};

#endif

///////////////////////////////////////////////////////////////////
// STOPWATCH
enum class StopwatchType
{
    STOPWATCH_WITH_CPUID_AND_RDTSC,
    STOPWATCH_WITH_RDTSCP
};

template <StopwatchType type = StopwatchType::STOPWATCH_WITH_RDTSCP>
class Stopwatch
{
public:

    static unsigned long long cpu_cycles_to_nanoseconds(unsigned long long cycle_count, unsigned long long cpu_frequency_hertz)
    {
        // cpu_frequency is number of cycles per 1 sec/1000000000 nanoseconds
        // So each cycle takes 1000000000/cpu_frequency_hertz nanoseconds
        double time_per_cycle = 1000000000.0 / static_cast<double>(cpu_frequency_hertz);
        return  static_cast<unsigned long long>(static_cast<double>(cycle_count) * time_per_cycle);
    }

    void start()
    {
        m_start_cycles = get_cycles();
    }

    void stop()
    {
        m_end_cycles = get_cycles();
    }

    unsigned long long get_elapsed_cycles()
    {

        return m_end_cycles - m_start_cycles;
    }

    unsigned long long get_elapsed_nanoseconds(unsigned long long cpu_frequency_hertz)
    {
        return  cpu_cycles_to_nanoseconds(get_elapsed_cycles(), cpu_frequency_hertz);
    }

private:
    unsigned long long m_start_cycles = 0;
    unsigned long long m_end_cycles = 0;

    unsigned long long get_cycles()
    {
        if constexpr (type == StopwatchType::STOPWATCH_WITH_RDTSCP)
        {
            return ProcessorUtilities::rdtscp();
        }
        else if constexpr (type == StopwatchType::STOPWATCH_WITH_CPUID_AND_RDTSC)
        {
            // Executes serialising cpuid instruction
            unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
            ProcessorUtilities::cpuid(0, &eax, &ebx, &ecx, &edx);
            return ProcessorUtilities::rdtsc();
        }
    }
};

///////////////////////////////////////////////////////////////////
// Statistics
template <typename T = double>
class Statistics
{
public:

    void reset()
    {
        m_samples.clear();
    }

    void add_sample(double n)
    {
        m_samples.push_back(n);
    }

    T get_average() const
    {
        auto sample_number = m_samples.size();
        if (!sample_number)
        {
            return -1.0;
        }
        return std::accumulate(m_samples.begin(), m_samples.end(), 0.0) / sample_number;
    }

    T get_minimum() const
    {
        if (!m_samples.size())
        {
            return -1.0;
        }
        return *std::min_element(std::begin(m_samples), std::end(m_samples));
    }

    T get_maximum() const
    {
        if (!m_samples.size())
        {
            return -1.0;
        }
        return *std::max_element(std::begin(m_samples), std::end(m_samples));
    }

    // Note : It sorts the samples
    T get_percentile(int percentile)
    {
        auto sample_number = m_samples.size();

        if (!sample_number)
        {
            return -1.0;
        }

        std::sort(m_samples.begin(), m_samples.end());

        std::size_t index = sample_number * percentile / 100;

        return m_samples[index];
    }

    void print(const std::string& title, const std::string& unit = "nanoseconds")
    {
        std::cout << std::endl;

        Console::print_colour(ConsoleColour::FG_BLUE, "Title : " + title);
        std::cout << std::endl;

        Console::print_colour(ConsoleColour::FG_GREEN, "Sample count : " + std::to_string(get_sample_count()));
        std::cout << std::endl;

        Console::print_colour(ConsoleColour::FG_YELLOW, "Minimum : ");
        std::cout << get_minimum() << " " << unit <<  std::endl;

        Console::print_colour(ConsoleColour::FG_YELLOW, "Maximum : ");
        std::cout << get_maximum() << " " << unit << std::endl;

        Console::print_colour(ConsoleColour::FG_YELLOW, "Average : ");
        Console::print_colour(ConsoleColour::FG_RED, std::to_string(get_average()) + " " + unit);
        std::cout << std::endl;

        Console::print_colour(ConsoleColour::FG_YELLOW, "P50 : ");
        std::cout << std::to_string(get_percentile(50)) << " " << unit << std::endl;

        Console::print_colour(ConsoleColour::FG_YELLOW, "P75 : ");
        std::cout << std::to_string(get_percentile(75)) << " " << unit << std::endl;

        Console::print_colour(ConsoleColour::FG_YELLOW, "P90 : ");
        std::cout << std::to_string(get_percentile(90)) << " " << unit << std::endl;

        Console::print_colour(ConsoleColour::FG_YELLOW, "P95 : ");
        std::cout << std::to_string(get_percentile(95)) << " " << unit << std::endl;

        Console::print_colour(ConsoleColour::FG_YELLOW, "P99 : ");
        std::cout << std::to_string(get_percentile(99)) << " " << unit << std::endl;

        std::cout << std::endl;
    }

    std::size_t get_sample_count() const
    {
        return m_samples.size();
    }

private:
    std::vector<T> m_samples;
};

///////////////////////////////////////////////////////////////////
// RandomNumberGenerator
class RandomNumberGenerator
{
    public:
        static int get_random_positive_integer(int max_random_number=100)
        {
            static thread_local std::default_random_engine rng{std::random_device{}()};
            return std::uniform_int_distribution<int>(1, max_random_number)(rng);
        }
};

///////////////////////////////////////////////////////////////////
// DO_NOT_OPTIMISE
inline void dummy(char const volatile*){}

template <typename T>
void DO_NOT_OPTIMISE(T const& value)
{
    // Disables compiler reordering of read and writes ( though it does not prevent CPU reordering )
    #if defined(_MSC_VER)
    dummy(&reinterpret_cast<char const volatile&>(value));
    _ReadWriteBarrier();
    #elif defined(__GNUC__)
    asm volatile("" : : "r,m"(value) : "memory");
    #endif
}

///////////////////////////////////////////////////////////////////
// BENCHMARK_BEGIN & BENCHMARK_END
#define STOPWATCH_TYPE StopwatchType::STOPWATCH_WITH_RDTSCP

#define BENCHMARK_BEGIN(iteration_count)  \
                            Statistics<double> report; Stopwatch<STOPWATCH_TYPE> stopwatch; auto cpu_frequency = ProcessorUtilities::get_current_cpu_frequency_hertz(); \
                            Console::print_colour(ConsoleColour::FG_YELLOW, "Current CPU frequency ( not min or max ) : " + std::to_string(cpu_frequency) + " Hz" ); std::cout << std::endl; \
                            for (std::size_t iteration{ 0 }; iteration < iteration_count; iteration++){ \
                            stopwatch.start(); \

#define BENCHMARK_END()  \
                            stopwatch.stop();     \
                            report.add_sample(static_cast<double>(stopwatch.get_elapsed_nanoseconds(cpu_frequency))); \
                            } \

#endif