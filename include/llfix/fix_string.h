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
    - ONLY TO BE USED FOR OUTGOING MESSAGES
    - USES HEAP AS IT WILL BE USED AS PART OF OUTGOING MESSAGES AND OUTGOING MESSAGES WILL BE POOLED. SO NOT USING STACK TO AVOID S.OVERFLOWS
*/
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <string>

#include "core/compiler/hints_branch_predictor.h"
#include "core/compiler/builtin_functions.h"
#include "core/utilities/allocator.h"

namespace llfix
{

class FixString
{
    public:

        FixString()
        {
            m_buffer = static_cast<char*>(Allocator::allocate(INITIAL_CAPACITY));

            if(m_buffer == nullptr)
            {
                throw std::bad_alloc();
            }

            m_buffer_capacity = INITIAL_CAPACITY;

            llfix_builtin_memset(m_buffer, 0, INITIAL_CAPACITY);
        }

        ~FixString()
        {
            if(m_buffer)
            {
                Allocator::deallocate(m_buffer, m_buffer_capacity);
            }
        }

        void copy_from(char val)
        {
            m_buffer[0] = val;
            m_length = 1;
        }

        void copy_from(const std::string_view text)
        {
            auto buffer_length = std::size(text);

            if (llfix_unlikely(buffer_length > m_buffer_capacity))
            {
                Allocator::deallocate(m_buffer, m_buffer_capacity);

                m_buffer = static_cast<char*>(Allocator::allocate(buffer_length+1));
                m_buffer_capacity = static_cast<uint32_t>(buffer_length + 1);

                if (m_buffer == nullptr)
                {
                    throw std::bad_alloc();
                }
            }

            llfix_builtin_memcpy(m_buffer, text.data(), buffer_length);
            m_length = static_cast<uint32_t>(buffer_length);
        }

        void copy_from(const char* buffer, std::size_t buffer_length)
        {
            if (llfix_unlikely(buffer_length > m_buffer_capacity))
            {
                Allocator::deallocate(m_buffer, m_buffer_capacity);

                m_buffer = static_cast<char*>(Allocator::allocate(buffer_length+1));
                m_buffer_capacity = static_cast<uint32_t>(buffer_length + 1);

                if (m_buffer == nullptr)
                {
                    throw std::bad_alloc();
                }
            }

            llfix_builtin_memcpy(m_buffer, buffer, buffer_length);
            m_length = static_cast<uint32_t>(buffer_length);
        }

        uint32_t length() const
        {
            return m_length;
        }

        char* data()
        {
            return m_buffer;
        }

        const char* c_str()
        {
            return m_buffer;
        }

        void set_length(uint32_t n)
        {
            m_length = n;
        }

        std::string_view to_string_view() const
        {
            return std::string_view(m_buffer, m_length);
        }

        std::string to_string() const
        {
            return std::string(m_buffer, m_length);
        }

        uint32_t capacity() const
        {
            return m_buffer_capacity;
        }

    private:
        char* m_buffer = nullptr;
        uint32_t m_buffer_capacity = 0;
        uint32_t m_length = 0;

        static constexpr uint32_t INITIAL_CAPACITY = 64;

        FixString(const FixString& other) = delete;
        FixString& operator= (const FixString& other) = delete;
        FixString(FixString&& other) = delete;
        FixString& operator=(FixString&& other) = delete;
};

} // namespace