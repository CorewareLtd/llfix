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
    - Linux provides VDSO to avoid a full syscall so you can stay in user space/mode while calling VDSO functions :

        https://man7.org/linux/man-pages/man7/vdso.7.html

    - Windows doesn`t provide a userspace syscall interface

    - PTP syncronisation : Not using function accessing PTP sources explicitly. Instead, we rely on system configuration.
                         If the host system configured for PTP sync, the APIs used will also return PTP synced values.

                         For ex: Linux PTP and Solarflare's sfptpd daemons use clock_adjtime() to control/adjust the system clock :
                         Solarflare Enhanced PTP User Guide & https://github.com/search?q=repo%3Arichardcochran%2Flinuxptp%20clock_adjtime&type=code

                            - clock_gettime with CLOCK_REALTIME (Linux)
                            - GetSystemTimePreciseAsFileTime (Windows)
                            - std::chrono::system_clock::now
*/
#include <ctime>
#include <cstdint>
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

#ifdef __linux__ // VOLTRON_EXCLUDE
#include <sys/time.h>
#elif _WIN32 // VOLTRON_EXCLUDE
#include <windows.h>
#endif // VOLTRON_EXCLUDE

namespace llfix
{

class VDSO
{
    public:

        enum class SubsecondPrecision
        {
            NANOSECONDS,
            MICROSECONDS,
            MILLISECONDS,
            NONE
        };

        static SubsecondPrecision string_to_subsecond_precision(const std::string& val)
        {
            SubsecondPrecision ret= SubsecondPrecision::NONE;

            if(val == "NANO")
            {
                ret = SubsecondPrecision::NANOSECONDS;
            }
            else if (val == "MICRO")
            {
                ret = SubsecondPrecision::MICROSECONDS;
            }
            else if (val == "MILLI")
            {
                ret = SubsecondPrecision::MILLISECONDS;
            }

            return ret;
        }

        // Format : YYYYMMDD-HH:MM:SS.123456789
        template<bool utc, SubsecondPrecision precision = SubsecondPrecision::NANOSECONDS>
        static void get_datetime_as_string(char* buffer)
        {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);

            long long subseconds = 0;

            if constexpr (precision == SubsecondPrecision::NANOSECONDS)
            {
                auto ns_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
                subseconds = ns_since_epoch.count() % 1'000'000'000;
            }
            else if constexpr (precision == SubsecondPrecision::MICROSECONDS)
            {
                auto us_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
                subseconds = us_since_epoch.count() % 1'000'000;
            }
            else if constexpr (precision == SubsecondPrecision::MILLISECONDS)
            {
                auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
                subseconds = ms_since_epoch.count() % 1'000;
            }

            std::tm tm_result;

            #ifdef __linux__
            if constexpr (utc)
            {
                gmtime_r(&t, &tm_result);
            }
            else
            {
                localtime_r(&t, &tm_result);
            }
            #elif _WIN32
            if constexpr (utc)
            {
                gmtime_s(&tm_result, &t);
            }
            else
            {
                localtime_s(&tm_result, &t);
            }
            #endif

            // Format date and time into buffer with nanoseconds
            char* target = buffer;

            auto write_two_digits = [&target](int val)
            {
                target[0] = '0' + val / 10;
                target[1] = '0' + val % 10;
            };

            auto write_four_digits = [&target](int val)
            {
                target[0] = '0' + (val / 1000) % 10;
                target[1] = '0' + (val / 100) % 10;
                target[2] = '0' + (val / 10) % 10;
                target[3] = '0' + (val % 10);
            };

            // Year
            write_four_digits(tm_result.tm_year + 1900);
            target += 4;
            // Month
            write_two_digits(tm_result.tm_mon + 1);
            target += 2;
            // Day
            write_two_digits(tm_result.tm_mday);
            target += 2;

            *target++ = '-';

            // Hour
            write_two_digits(tm_result.tm_hour);
            target += 2;
            *target++ = ':';

            // Minute
            write_two_digits(tm_result.tm_min);
            target += 2;
            *target++ = ':';

            // Seconds
            write_two_digits(tm_result.tm_sec);
            target += 2;

            // Subseconds
            if constexpr (precision == SubsecondPrecision::NANOSECONDS)
            {
                *target++ = '.';
                for (int i = 100'000'000; i > 0; i /= 10)
                {
                    *target++ = '0' + (subseconds / i) % 10;
                }
            }
            else if constexpr (precision == SubsecondPrecision::MICROSECONDS)
            {
                *target++ = '.';
                for (int i = 100'000; i > 0; i /= 10)
                {
                    *target++ = '0' + (subseconds / i) % 10;
                }
            }
            else if constexpr (precision == SubsecondPrecision::MILLISECONDS)
            {
                *target++ = '.';
                for (int i = 100; i > 0; i /= 10)
                {
                    *target++ = '0' + (subseconds / i) % 10;
                }
            }

            // Null termination
            *target = '\0';
        }

        static uint64_t nanoseconds_since_epoch()
        {
            #ifdef __linux__
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
            #elif _WIN32
            FILETIME ft;
            GetSystemTimePreciseAsFileTime(&ft);

            // Convert FILETIME to a 64-bit integer (100-nanosecond intervals since January 1, 1601)
            ULARGE_INTEGER ull;
            ull.LowPart = ft.dwLowDateTime;
            ull.HighPart = ft.dwHighDateTime;

            // Convert to nanoseconds since Unix epoch (January 1, 1970)
            uint64_t nanoseconds = (ull.QuadPart - 116'444'736'000'000'000ULL) * 100;
            return nanoseconds;
            #endif
        }

        static uint64_t nanoseconds_monotonic()
        {
            #ifdef __linux__
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
            #elif _WIN32
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            #endif
        }

        static void get_time_utc(int& hour, int& min)
        {
            #ifdef __linux__
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);

            time_t now = ts.tv_sec;
            struct tm gm;
            gmtime_r(&now, &gm);

            hour = gm.tm_hour;
            min = gm.tm_min;
            #elif _WIN32
            time_t now = time(nullptr);
            tm gm;
            gmtime_s(&gm, &now);

            hour = gm.tm_hour;
            min = gm.tm_min;
            #endif
        }

        // Monday=1 .... Sunday=7
        static int get_utc_current_weekday_number()
        {
            std::time_t t = std::time(nullptr);
            std::tm utc;

            #ifdef __linux__
            gmtime_r(&t, &utc);
            #elif _WIN32
            gmtime_s(&utc, &t);
            #endif

            int ret = utc.tm_wday; // Sunday = 0, Monday = 1, ... Saturday = 6

            if (ret == 0)
            {
                ret = 7; // make Sunday = 7
            }

            return ret;
        }

        // Not low latency, for offline deserialisers, output format : HH:MM:SS.123456789
        static std::string convert_nanoseconds_since_epoch_to_time_string(uint64_t nanosecs_since_epoch)
        {
            uint64_t total_seconds = nanosecs_since_epoch / 1'000'000'000ULL;
            uint64_t nanoseconds = nanosecs_since_epoch % 1'000'000'000ULL;

            uint64_t hours = (total_seconds / 3600) % 24;
            uint64_t minutes = (total_seconds / 60) % 60;
            uint64_t seconds = total_seconds % 60;

            std::ostringstream oss;
            oss << std::setw(2) << std::setfill('0') << hours << ":"
                << std::setw(2) << std::setfill('0') << minutes << ":"
                << std::setw(2) << std::setfill('0') << seconds << "."
                << std::setw(9) << std::setfill('0') << nanoseconds;

            return oss.str();
        }
};

} // namespace