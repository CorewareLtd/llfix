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

#include <cassert>
#include <string>
#include <unordered_map>

#include "../../core/os/vdso.h"

namespace llfix
{

class SessionScheduleValidator
{
    public:

        void add_allowed_weekdays_from_dash_separated_string(const std::string& weekdays)
        {
            bool looking_for_delimiter{ false };

            for (const auto& ch : weekdays)
            {
                if (looking_for_delimiter == false)
                {
                    add_allowed_weekday(ch - '0');
                }

                looking_for_delimiter = !looking_for_delimiter;
            }
        }

        void add_allowed_weekday(int weekday)
        {
            assert(weekday >= 1 && weekday <= 7);
            m_allowed_weekdays[weekday] = weekday;
        }

        void set_start_and_end_times(int start_hour_utc, int start_minute_utc, int end_hour_utc, int end_minute_utc)
        {
            if(start_hour_utc >= 0 && start_minute_utc >= 0 && end_hour_utc >= 0 && end_minute_utc >= 0) // If any of them is -1 ,then we assume that start and end times not configured
            {
                m_session_start_end_times_specified = true;

                m_start_time_in_minutes_utc = (start_hour_utc*60) + start_minute_utc;
                m_end_time_in_minutes_utc = (end_hour_utc*60) + end_minute_utc;
            }
            else
            {
                m_session_start_end_times_specified = false;
            }
        }

        bool is_now_valid_datetime() const
        {
            ////////////////////////////////////////////////////////////////////////////////////////////////
            // DATE
            if (m_allowed_weekdays.empty() == false)
            {
                if (m_allowed_weekdays.find(VDSO::get_utc_current_weekday_number()) == m_allowed_weekdays.end())
                {
                    return false;
                }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////
            // TIME
            if(m_session_start_end_times_specified==false)
            {
                return true; // If not specified we return as current time is a valid session time
            }

            int current_hour_utc, current_minute_utc;
            VDSO::get_time_utc(current_hour_utc, current_minute_utc);
            int now_in_minutes_utc = (current_hour_utc*60) + current_minute_utc;

            if(now_in_minutes_utc>= m_start_time_in_minutes_utc && now_in_minutes_utc < m_end_time_in_minutes_utc)
            {
                return true;
            }

            return false;
        }

    private:
        int m_start_time_in_minutes_utc=0;
        int m_end_time_in_minutes_utc=0;
        bool m_session_start_end_times_specified = false;
        std::unordered_map<int, int> m_allowed_weekdays;
};

} // namespace