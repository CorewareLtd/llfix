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
    - SLIDING WINDOW TYPE THROTTLER BASED ON A RING ( CIRCULAR ) BUFFER
    - CURRENTLY SUPPORTS ONLY SINGLE-LEVEL THROTTLER
    - NOT THREAD SAFE
*/
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "../../core/compiler/hints_hot_code.h"
#include "../../core/compiler/hints_branch_predictor.h"

#include "../../core/os/thread_utilities.h"
#include "../../core/os/vdso.h"

#include "../../core/utilities/ring_buffer.h"

namespace llfix
{

class Throttler
{
    public:

        bool initialise(uint64_t window_in_nanoseconds, std::size_t limit)
        {
            assert(limit>0);
            assert(window_in_nanoseconds >= 1000);

            m_limit = limit;
            m_window_in_nanoseconds = window_in_nanoseconds;

            if (m_event_times.create(m_limit) == false)
            {
                return false;
            }

            // Pre-fill the ring_buffer with stale values
            const auto now = VDSO::nanoseconds_monotonic();
            const auto past = (now > m_window_in_nanoseconds) ? (now - m_window_in_nanoseconds) : 0;
            for (std::size_t i = 0; i < m_limit; ++i)
            {
                m_event_times.push(past);
            }

            return true;
        }

        LLFIX_FORCE_INLINE void update()
        {
            auto timestamp_latest = VDSO::nanoseconds_monotonic();
            m_latest_event_time_index = m_event_times.push(timestamp_latest);

            auto first_message_index = m_latest_event_time_index + 1;
            first_message_index = first_message_index >= m_limit ? (first_message_index - m_limit) : (first_message_index);
            auto timestamp_first = m_event_times.get(first_message_index);

            auto delta_time = timestamp_latest - timestamp_first;
            m_wait_time_nano_seconds = (delta_time < m_window_in_nanoseconds) ? m_window_in_nanoseconds-delta_time : 0;
        }

        LLFIX_FORCE_INLINE void wait()
        {
            if (m_wait_time_nano_seconds>0)
            {
                ThreadUtilities::sleep_in_nanoseconds(static_cast<uint64_t>(m_wait_time_nano_seconds));
                m_event_times.set(m_latest_event_time_index, VDSO::nanoseconds_monotonic());

                #ifdef LLFIX_UNIT_TEST
                trigger_count++;
                last_wait_time_nsecs = m_wait_time_nano_seconds;
                #endif
            }
        }

        LLFIX_FORCE_INLINE bool reached_limit() const { return m_wait_time_nano_seconds>0; }

    private:
        RingBuffer<uint64_t> m_event_times;
        std::size_t m_limit = 0;
        uint64_t m_window_in_nanoseconds = 0;

        uint64_t m_wait_time_nano_seconds = 0;
        std::size_t m_latest_event_time_index = 0;

    #ifdef LLFIX_UNIT_TEST
    public:
        std::size_t trigger_count = 0;
        uint64_t last_wait_time_nsecs = 0;
    #endif
};

} // namespace