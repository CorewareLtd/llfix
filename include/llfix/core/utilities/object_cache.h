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
    - LINEAR OBJECT CACHE LIKE BUMP ALLOCATOR, USE ONLY IF YOU CAN RELEASE ALL AT ONCE
    - NOT THREAD SAFE
    - IF THE ALLOCATED CAPACITY IS INSUFFICIENT , DOUBLES THE CAPACITY
    - SUPPORTS ONLY CTORS WITHOUT ARGUMENTS
*/
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "../compiler/hints_branch_predictor.h"
#include "allocator.h"

namespace llfix
{

template <typename T, typename AllocatorType=Allocator>
class ObjectCache
{
    public:

        bool create(std::size_t initial_capacity)
        {
            m_cache.reserve(initial_capacity);
            return grow(initial_capacity);
        }

        ~ObjectCache()
        {
            destroy();
        }

        T* allocate()
        {
            assert(m_cached_object_count>0);

            if(llfix_unlikely(m_pointer >= m_cached_object_count))
            {
                // Doubling the capacity
                if( grow(m_cached_object_count) == false )
                {
                    return nullptr;
                }
            }

            auto ret = m_cache[m_pointer];
            m_pointer++;
            return ret;
        }

        void decrement_pointer()
        {
            assert(m_pointer>0);
            m_pointer--;
        }

        void reset_pointer()
        {
            m_pointer = 0;
        }

    private:
        std::vector<T*> m_cache;
        std::size_t m_pointer=0;
        std::size_t m_cached_object_count = 0;

        [[nodiscard]] bool grow(std::size_t count)
        {
            for(std::size_t i=0; i<count;i++)
            {
                T* object = reinterpret_cast<T*>(AllocatorType::allocate(sizeof(T)));

                if(object == nullptr)
                {
                    return false;
                }

                if constexpr (std::is_constructible<T>::value)
                {
                    new (object) T();
                }

                m_cache.push_back(object);
                m_cached_object_count++;
            }

            return true;
        }

        void destroy()
        {
            for(auto& ptr:m_cache)
            {
                if(ptr)
                {
                    if constexpr (std::is_destructible<T>::value)
                    {
                        reinterpret_cast<T*>(ptr)->~T();
                    }

                    AllocatorType::deallocate(ptr, sizeof(T));
                }
            }
        }
};

} // namespace