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
// NON-THREAD SAFE SIMPLE RING/CIRCULAR BUFFER, USED IN THROTTLING , NOT AN SPSC LOCKFREE DS

#include <cassert>
#include <cstddef>
#include "../compiler/builtin_functions.h"
#include "allocator.h"

namespace llfix
{

template <typename T, typename Allocator = Allocator>
class RingBuffer
{
    public:
        bool create(std::size_t capacity)
        {
            destroy();

            m_index=0;
            m_capacity = capacity;
            m_buffer = reinterpret_cast<T*>(Allocator::allocate(m_capacity * sizeof(T)) );

            if (m_buffer == nullptr)
            {
                return false;
            }

            llfix_builtin_memset(m_buffer, 0, m_capacity * sizeof(T));

            return true;
        }

        ~RingBuffer()
        {
            destroy();
        }

        std::size_t push(T t)
        {
            std::size_t new_item_index = calculate_index();
            m_buffer[new_item_index] = t;
            m_index++;
            return new_item_index;
        }

        T get(std::size_t index)
        {
            return m_buffer[index];
        }

        void set(std::size_t index, T val)
        {
            m_buffer[index] = val;
        }

        auto begin()
        {
            return static_cast<T*>(m_buffer);
        }

        auto end()
        {
            return static_cast<T*>(m_buffer) + m_capacity;
        }

        std::size_t capacity () const { return m_capacity; }

    private:
        T* m_buffer = nullptr;
        std::size_t m_capacity = 0;
        std::size_t m_index = 0;

        void destroy()
        {
            if (m_buffer)
            {
                Allocator::deallocate(m_buffer, m_capacity * sizeof(T));
                m_buffer = nullptr;
            }
        }

        std::size_t calculate_index()
        {
            assert(m_capacity>0);
            return m_index - (m_index / m_capacity) * m_capacity;
        }
};

} // namespace