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

/*
    TTAS : test-test-and-set ( https://en.wikipedia.org/wiki/Test_and_test-and-set ) to reduce cache-coherency traffic
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

#include "../compiler/hints_hot_code.h"
#include "../cpu/alignment_constants.h"
#include "../cpu/pause.h"
#include "../os/thread_utilities.h"

namespace llfix
{

template<std::size_t alignment=AlignmentConstants::CPU_CACHE_LINE_SIZE, std::size_t pause_count = 1, bool extra_system_friendly = false>
struct UserspaceSpinlock
{
    LLFIX_ALIGN_DATA(alignment) std::atomic<bool> m_flag{false}; // To avoid false sharing and misaligned accesses

    void initialise()
    {
        m_flag.store(false, std::memory_order_relaxed);
    }

    void lock()
    {
        while (true)
        {
            if (!m_flag.exchange(true, std::memory_order_acquire))
            {
                return;
            }

            if constexpr (extra_system_friendly)
            {
                ThreadUtilities::yield();
            }

            while (m_flag.load(std::memory_order_relaxed))
            {
                pause(pause_count);
            }
        }
    }

    LLFIX_FORCE_INLINE bool try_lock()
    {
        return !m_flag.load(std::memory_order_relaxed) && !m_flag.exchange(true, std::memory_order_acquire);
    }

    LLFIX_FORCE_INLINE void unlock()
    {
        m_flag.store(false, std::memory_order_release);
    }
};

} // namespace