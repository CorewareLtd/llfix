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

#include <cstdint>
#include <cstddef>
#include <string_view>

#include "../../core/os/memory_mapped_file.h"
#include "../../core/utilities/filesystem_utilities.h"

namespace llfix
{

/**
 * @class SequenceStore
 * @brief Persistent FIX sequence number store backed by a memory-mapped file.
 *
 * Maintains incoming and outgoing FIX sequence numbers and persists them
 * to disk using a memory-mapped file to survive process restarts.
 *
 */
class SequenceStore
{
    public:

        ~SequenceStore()
        {
            save_to_disc();
            m_memory_mapped_file.close();
        }

        bool open(const std::string_view& file_path)
        {
            auto folder = FileSystemUtilities::get_path_by_excluding_file(file_path);

            if (folder.length() > 0)
            {
                if (FileSystemUtilities::does_path_exist(folder) == false)
                {
                    if (FileSystemUtilities::create_directory(folder) == false)
                    {
                        return false;
                    }
                }
            }

            bool success = m_memory_mapped_file.open(file_path, MEMORY_MAPPED_FILE_SIZE);

            if (success)
            {
                m_buffer = nullptr;
                std::size_t buffer_size = 0;
                m_memory_mapped_file.get_buffer(&m_buffer, buffer_size);
            }

            return success;
        }

        bool is_open() const
        {
            return m_memory_mapped_file.is_open();
        }

        void save_to_disc()
        {
            if (m_memory_mapped_file.is_open())
            {
                m_memory_mapped_file.flush_to_disc();
            }
        }

        /**
        * @brief Reset both incoming and outgoing sequence numbers to zero.
        */
        void reset_numbers()
        {
            set_outgoing_seq_no(0);
            set_incoming_seq_no(0);
        }

        // OUTGOING
        void increment_outgoing_seq_no()
        {
            reinterpret_cast<uint32_t*>(m_buffer)[0]++;
        }

        void decrement_outgoing_seq_no()
        {
            reinterpret_cast<uint32_t*>(m_buffer)[0]--;
        }

        /**
        * @brief Set the outgoing FIX sequence number.
        * @param n New outgoing sequence number
        */
        void set_outgoing_seq_no(uint32_t n)
        {
            reinterpret_cast<uint32_t*>(m_buffer)[0] = n;
        }

        /**
        * @brief Get the outgoing FIX sequence number.
        * @return Current outgoing sequence number
        */
        uint32_t get_outgoing_seq_no() const
        {
            return reinterpret_cast<uint32_t*>(m_buffer)[0];
        }

        // INCOMING
        void increment_incoming_seq_no()
        {
            reinterpret_cast<uint32_t*>(m_buffer)[1]++;
        }

        /**
        * @brief Set the incoming FIX sequence number.
        * @param n New incoming sequence number
        */
        void set_incoming_seq_no(uint32_t n)
        {
            reinterpret_cast<uint32_t*>(m_buffer)[1] = n;
        }

        /**
        * @brief Get the incoming FIX sequence number.
        * @return Current incoming sequence number
        */
        uint32_t get_incoming_seq_no() const
        {
            return reinterpret_cast<uint32_t*>(m_buffer)[1];
        }

    protected:
        MemoryMappedFile m_memory_mapped_file;
        char* m_buffer = nullptr;
        static inline constexpr std::size_t MEMORY_MAPPED_FILE_SIZE = 65536; // Even though a typical vm page is 4KB, on Windows page allocation granularity is 64KB
};

} // namespace