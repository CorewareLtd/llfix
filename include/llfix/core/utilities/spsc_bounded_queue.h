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
    - Does not support object construction/destruction

    - Bounded ring buffer single producer single consumer lockfree queue

    - Since it is bounded choosing a good capacity is essential to minimise contention

    - Payload buffer start address will be CPU cache line size aligned with help of oversized allocation/padding.
      However to avoid false sharing , the sizeof(T) should be a multiple of CPU cache line size

    - Using cached read and write index to avoid cache misses
*/
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "../compiler/hints_hot_code.h"
#include "../cpu/alignment_constants.h"

#include "allocator.h"

namespace llfix
{

template <typename T, typename Allocator = Allocator>
class SPSCBoundedQueue
{
    public :

        SPSCBoundedQueue() = default;

        ~SPSCBoundedQueue()
        {
            if (m_buffer)
            {
                Allocator::deallocate(m_buffer, sizeof(T) * m_capacity + AlignmentConstants::CPU_CACHE_LINE_SIZE);
            }
        }

        SPSCBoundedQueue(const SPSCBoundedQueue& other) = delete;
        SPSCBoundedQueue& operator= (const SPSCBoundedQueue& other) = delete;
        SPSCBoundedQueue(SPSCBoundedQueue&& other) = delete;
        SPSCBoundedQueue& operator=(SPSCBoundedQueue&& other) = delete;

        bool create(std::size_t capacity)
        {
            assert(capacity > 0);

            auto allocation_size = sizeof(T) * capacity + AlignmentConstants::CPU_CACHE_LINE_SIZE; // Oversized allocation in order to have a CPU-cache-line-size aligned start address

            m_buffer = static_cast<T*>(Allocator::allocate(allocation_size));

            if (m_buffer == nullptr)
            {
                return false;
            }

            std::size_t alignment_padding_offset = AlignmentConstants::CPU_CACHE_LINE_SIZE - (modulo_pow2((reinterpret_cast<std::uint64_t>(m_buffer)), AlignmentConstants::CPU_CACHE_LINE_SIZE));
            m_cache_line_aligned_buffer =  reinterpret_cast<T*>((reinterpret_cast<char*>(m_buffer) + alignment_padding_offset));

            m_capacity = capacity;

            return true;
        }

        void push(T val)
        {
            assert(m_capacity > 0);
            while (!try_push(val));
        }

        [[nodiscard]] bool try_push(T val)
        {
            assert(m_capacity > 0);

            uint64_t current_write_index = m_write_index.load(std::memory_order_relaxed);
            uint64_t next_write_index = current_write_index + 1;

            ////////////////////////////////////////////////////////////////////////////////////////////////////////
            // READ INDEX SYNC POINT: Ensures visibility of consumer's read index and any memory writes
            // prior to this point by the consumer. Waits until consumer releases (updates) the read index.
            if (current_write_index - m_cached_read_index.load(std::memory_order_relaxed) >= m_capacity)
            {
                // Synchronize with the consumer's read index
                m_cached_read_index.store(m_read_index.load(std::memory_order_acquire), std::memory_order_relaxed);
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////

            if (current_write_index == m_cached_read_index.load(std::memory_order_relaxed) + m_capacity)
            {
                return false; // Queue is full
            }

            m_cache_line_aligned_buffer[modulo_capacity(current_write_index)] = val;

            ////////////////////////////////////////////////////////////////////////////////////////////////////////
            // WRITE INDEX SYNC POINT: Ensures the visibility of the value write in the buffer to the consumer.
            // Allows the consumer to acquire the updated write index and see the value.
            m_write_index.store(next_write_index, std::memory_order_release);
            ////////////////////////////////////////////////////////////////////////////////////////////////////////

            return true;
        }

        [[nodiscard]] bool try_pop(T* element)
        {
            assert(m_capacity > 0);

            uint64_t current_read_index = m_read_index.load(std::memory_order_relaxed);

            ////////////////////////////////////////////////////////////////////////////////////////////////////////
            // WRITE INDEX SYNC POINT: Ensures visibility of the producer's write index and any memory writes
            // (buffer updates) prior to this point by the producer. Waits until producer releases (updates) the write index.
            if (current_read_index == m_cached_write_index.load(std::memory_order_relaxed))
            {
                // Synchronize with the producer's write index
                m_cached_write_index.store(m_write_index.load(std::memory_order_acquire), std::memory_order_relaxed);
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////////////

            if (current_read_index == m_cached_write_index.load(std::memory_order_relaxed))
            {
                return false; // Queue is empty
            }

            *element = m_cache_line_aligned_buffer[modulo_capacity(current_read_index)];

            ////////////////////////////////////////////////////////////////////////////////////////////////////////
            // READ INDEX SYNC POINT: Ensures the visibility of the updated read index to the producer.
            // Allows the producer to acquire the updated read index and continue writing.
            m_read_index.store(current_read_index + 1, std::memory_order_release);
            ////////////////////////////////////////////////////////////////////////////////////////////////////////

            return true;
        }

        std::size_t size()
        {
            uint64_t current_write_index = m_write_index.load(std::memory_order_acquire);
            uint64_t current_read_index = m_read_index.load(std::memory_order_acquire);
            return current_write_index - current_read_index;
        }

    private :
        T* m_buffer = nullptr;
        T* m_cache_line_aligned_buffer = nullptr;
        std::size_t m_capacity = 0;

        LLFIX_ALIGN_DATA(AlignmentConstants::CPU_CACHE_LINE_SIZE) std::atomic<uint64_t> m_write_index = 0;
        LLFIX_ALIGN_DATA(AlignmentConstants::CPU_CACHE_LINE_SIZE) std::atomic<uint64_t> m_cached_write_index = 0;
        LLFIX_ALIGN_DATA(AlignmentConstants::CPU_CACHE_LINE_SIZE) std::atomic<uint64_t> m_read_index = 0;
        LLFIX_ALIGN_DATA(AlignmentConstants::CPU_CACHE_LINE_SIZE) std::atomic<uint64_t> m_cached_read_index = 0;

        LLFIX_FORCE_INLINE std::size_t modulo_capacity(std::size_t input) const
        {
            assert(m_capacity > 0);
            return input - (input / m_capacity) * m_capacity;
        }

        LLFIX_FORCE_INLINE static std::size_t modulo_pow2(std::size_t first, std::size_t second)
        {
            return first & (second - 1);
        }
};

} // namespace