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
    MEMORY MAPPED FILE LAYOUT :
                                MESSAGE COUNT [ 8 bytes ]

                                then for each message :

                                            timestamp  [ 8 bytes        ]
                                            success    [ 1 byte         ]
                                            msglen     [ 8 bytes        ]
                                            message    [ message length ]

*/
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <unordered_map>

#include "../../core/compiler/builtin_functions.h"
#include "../../core/compiler/packed.h"
#include "../../core/compiler/unused.h"
#include "../../core/compiler/hints_branch_predictor.h"

#include "../../core/os/memory_mapped_file.h"
#include "../../core/os/vdso.h"

#include "../../core/utilities/filesystem_utilities.h"
#include "../../core/utilities/serialisation_utilities.h"
#include "../../core/utilities/logger.h"

namespace llfix
{

LLFIX_PACKED
(
    struct MessageMemoryRecord
    {
        uint64_t offset = 0;
        uint64_t length = 0;
        int serialised_file_number = 0;
    }
);

template<typename MessageSequenceNumberExtractorType>
class MessageSerialiser
{
    public:

        MessageSerialiser()
        {
            static_assert(sizeof(MessageMemoryRecord) == 20);
        }

        ~MessageSerialiser()
        {
            close();
        }

        void close()
        {
            if (m_initialised)
            {
                flush();
                m_current_memory_mapping.close();
                m_initialised = false;
            }
        }

        bool initialise(const std::string_view& message_serialisation_path, std::size_t serialised_file_max_size, bool keep_message_records_in_memory, std::size_t message_records_initial_size=0)
        {
            close();

            m_keep_message_records_in_memory = keep_message_records_in_memory;

            if(m_keep_message_records_in_memory)
            {
                if (message_records_initial_size == 0)
                {
                    return false;
                }

                m_message_memory_records.reserve(message_records_initial_size);
            }

            if (serialised_file_max_size < 4096 || serialised_file_max_size % 4096 != 0) // 4096 -> typical VM Page size
            {
                return false;
            }

            m_serialised_file_max_size = serialised_file_max_size;
            std::size_t buffer_size = 0;

            bool folder_exists = m_serialisation_folder.initialise(message_serialisation_path);

            if(folder_exists)
            {
                m_current_serialised_file_number = m_serialisation_folder.get_latest_serialised_file_number();

                if(m_keep_message_records_in_memory && m_current_serialised_file_number>1) // We skip 1.bin as process_existing_buffer will get called for the latest file after the loop
                {
                    for(int i =1 ; i<= m_current_serialised_file_number-1; i++) // -1 due to process_existing_buffer will get called for the latest file after the loop
                    {
                        if (m_current_memory_mapping.open(m_serialisation_folder.get_serialised_file_path(i), m_serialised_file_max_size) == false)
                        {
                            return false;
                        }

                        m_current_memory_mapping.get_buffer(&m_current_memory_mapping_buffer, buffer_size);

                        // Builds m_message_records
                        if (process_existing_buffer(m_current_memory_mapping_buffer, buffer_size, m_current_memory_mapping_no_of_messages, m_current_memory_mapping_written_bytes, true, i) == false)
                        {
                            return false;
                        }

                        m_current_memory_mapping.close();
                    }
                }

                if (m_current_serialised_file_number == -1)
                {
                    m_current_serialised_file_number = 1;
                }

                if (m_current_memory_mapping.open(m_serialisation_folder.get_serialised_file_path(m_current_serialised_file_number), m_serialised_file_max_size) == false)
                {
                    return false;
                }

                m_current_memory_mapping.get_buffer(&m_current_memory_mapping_buffer, buffer_size);

                // Updates m_current_memory_mapping_no_of_messages & m_current_memory_mapping_written_bytes and builds m_message_records
                if (process_existing_buffer(m_current_memory_mapping_buffer, buffer_size, m_current_memory_mapping_no_of_messages, m_current_memory_mapping_written_bytes, m_keep_message_records_in_memory, m_current_serialised_file_number) == false)
                {
                    return false;
                }
            }
            else
            {
                if (FileSystemUtilities::create_directory(message_serialisation_path.data()) == false)
                {
                    return false;
                }

                m_current_serialised_file_number = 1;

                if (m_current_memory_mapping.open(m_serialisation_folder.get_serialised_file_path(m_current_serialised_file_number), m_serialised_file_max_size) == false)
                {
                    return false;
                }

                m_current_memory_mapping.get_buffer(&m_current_memory_mapping_buffer, buffer_size);

                llfix_builtin_memcpy(m_current_memory_mapping_buffer, &m_current_memory_mapping_no_of_messages, 8);
                m_current_memory_mapping_written_bytes = 8; // First 8 bytes will have the no of messages
            }

            m_initialised = true;

            return true;
        }

        void write(const void* message, std::size_t message_size, bool successful_transmission, uint32_t sequence_no=0)
        {
            if(llfix_unlikely(m_current_memory_mapping_buffer == nullptr))
            {
                return;
            }

            if (((m_serialised_file_max_size - m_current_memory_mapping_written_bytes)) < (message_size+m_serialisation_overhead_bytes))
            {
                // WE NEED TO SWITCH TO THE NEXT MEMORY MAPPED FILE
                if(llfix_unlikely(switch_to_next_memory_mapped_file() == false))
                {
                    return;
                }

                if (((m_serialised_file_max_size - m_current_memory_mapping_written_bytes)) < (message_size+m_serialisation_overhead_bytes))
                {
                    return;
                }
            }

            // WRITE TIMESTAMP
            uint64_t epoch = VDSO::nanoseconds_since_epoch();
            llfix_builtin_memcpy(m_current_memory_mapping_buffer + m_current_memory_mapping_written_bytes, &epoch, 8);
            m_current_memory_mapping_written_bytes += 8;

            // WRITE SUCCESS FLAG
            char successful_transmission_value = successful_transmission ? 1 : 0;
            llfix_builtin_memcpy(m_current_memory_mapping_buffer + m_current_memory_mapping_written_bytes, &successful_transmission_value, 1);
            m_current_memory_mapping_written_bytes += 1;

            // WRITE MSGLEN
            llfix_builtin_memcpy(m_current_memory_mapping_buffer + m_current_memory_mapping_written_bytes, &message_size, 8);
            m_current_memory_mapping_written_bytes += 8;

            if (m_keep_message_records_in_memory)
            {
                MessageMemoryRecord record;
                record.serialised_file_number = m_current_serialised_file_number;
                record.length = message_size;
                record.offset = m_current_memory_mapping_written_bytes;
                m_message_memory_records.insert({ sequence_no, record }); // We intentionally insert only unique sequence numbers
            }

            // WRITE MESSAGE
            llfix_builtin_memcpy(m_current_memory_mapping_buffer + m_current_memory_mapping_written_bytes, message, message_size);
            m_current_memory_mapping_written_bytes += message_size;

            // Update no of messages
            m_current_memory_mapping_no_of_messages++;
            llfix_builtin_memcpy(m_current_memory_mapping_buffer, &m_current_memory_mapping_no_of_messages, 8);
        }

        void flush()
        {
            m_current_memory_mapping.flush_to_disc();
        }

        bool has_message_in_memory(uint32_t sequence_no)
        {
            if(m_keep_message_records_in_memory==false)
            {
                return false;
            }

            if (m_message_memory_records.find(sequence_no) == m_message_memory_records.end())
            {
                return false;
            }

            return true;
        }

        std::size_t get_message_record_count() const
        {
            return m_message_memory_records.size();
        }

        bool read_message(uint32_t sequence_no, char* buffer, std::size_t buffer_size, std::size_t& message_length)
        {
            if(has_message_in_memory(sequence_no) == false)
            {
                return false;
            }

            auto record = m_message_memory_records[sequence_no];

            if (buffer_size < record.length)
            {
                return false;
            }

            if (m_current_serialised_file_number != record.serialised_file_number)
            {
                MemoryMappedFile target_file;
                auto target_memory_mapped_file_path = m_serialisation_folder.get_serialised_file_path(record.serialised_file_number);

                if (target_file.open(target_memory_mapped_file_path, m_serialised_file_max_size) == false)
                {
                    return false;
                }

                char* read_buffer{ nullptr };
                std::size_t read_buffer_size = 0;

                target_file.get_buffer(&read_buffer, read_buffer_size);

                llfix_builtin_memcpy(buffer, read_buffer + record.offset, record.length);

                target_file.close();
            }
            else
            {
                llfix_builtin_memcpy(buffer, m_current_memory_mapping_buffer + record.offset, record.length);
            }

            message_length = record.length;

            return true;
        }

        template <typename CB>
        static bool deserialise(char* buffer, std::size_t buffer_size, uint64_t& message_count, std::size_t& read_bytes, CB cb)
        {
            std::size_t buffer_offset = 0;
            uint64_t read_message_count = 0;

            message_count = *(reinterpret_cast<uint64_t*>(buffer));
            buffer_offset += sizeof(uint64_t);

            while (true && message_count != read_message_count)
            {
                // TIMESTAMP
                uint64_t current_timestamp = *(reinterpret_cast<uint64_t*>(buffer + buffer_offset));
                buffer_offset += sizeof(uint64_t);

                if (buffer_offset >= buffer_size)
                    break;

                // SUCCESS BYTE
                uint8_t current_success_flag = *(reinterpret_cast<uint8_t*>(buffer + buffer_offset));
                buffer_offset += 1;

                if (buffer_offset >= buffer_size)
                    break;

                // MESSAGE LENGTH
                uint64_t current_message_length = *(reinterpret_cast<uint64_t*>(buffer + buffer_offset));
                buffer_offset += sizeof(uint64_t);

                // MESSAGE
                std::string current_message;
                current_message.reserve(current_message_length);

                for (uint64_t i = 0; i < current_message_length; i++)
                {
                    current_message += buffer[buffer_offset + i];
                }

                read_message_count++;

                // CALLBACK
                cb(current_timestamp, (current_success_flag == static_cast<uint8_t>(1)) ? true : false, current_message_length, current_message, buffer_offset);

                buffer_offset += static_cast<std::size_t>(current_message_length);

                if (read_message_count == message_count)
                    break;

                if (buffer_offset >= buffer_size)
                    break;
            }

            read_bytes = buffer_offset;
            return read_message_count == message_count;
        }

    private:
        bool m_initialised = false;
        std::size_t m_serialised_file_max_size = 0;
        SerialisationUtilities::SerialisationFolder m_serialisation_folder;

        int m_current_serialised_file_number = -1;
        MemoryMappedFile m_current_memory_mapping;
        char* m_current_memory_mapping_buffer = nullptr;
        std::size_t m_current_memory_mapping_written_bytes = 0;
        uint64_t m_current_memory_mapping_no_of_messages = 0;

        std::unordered_map<uint32_t, MessageMemoryRecord> m_message_memory_records;
        bool m_keep_message_records_in_memory = false;

        // IMPORTANT : ANY TIME YOU ADD MORE CONTENT TO SERIALISATION , YOU NEED TO UPDATE THE LINE BELOW
        static constexpr inline std::size_t m_serialisation_overhead_bytes = 17; // 17 = 8 (epoch per message ) + 1 (transmission success) + 8 (msglen)

        bool switch_to_next_memory_mapped_file()
        {
            m_current_serialised_file_number++;

            m_current_memory_mapping.flush_to_disc();
            m_current_memory_mapping.close();

            auto file_path = m_serialisation_folder.get_serialised_file_path(m_current_serialised_file_number);

            if(m_current_memory_mapping.open(file_path, m_serialised_file_max_size) == false)
            {
                m_current_serialised_file_number--;
                LLFIX_LOG_FATAL("Failed to create message serialisation : " + file_path);
                m_current_memory_mapping_buffer = nullptr;
                return false;
            }

            std::size_t buffer_size = 0;
            m_current_memory_mapping.get_buffer(&m_current_memory_mapping_buffer, buffer_size);

            m_current_memory_mapping_written_bytes = 8; // First 8 bytes will have the no of messages
            m_current_memory_mapping_no_of_messages = 0;

            return true;
        }

        bool process_existing_buffer(char * buffer, std::size_t buffer_size, uint64_t& message_count, std::size_t& read_bytes, bool keep_message_records_in_memory, int serialised_file_number)
        {
            auto handler = [&](uint64_t timestamp, bool success, uint64_t msg_len, const std::string& message, std::size_t message_offset)
                {
                    LLFIX_UNUSED(timestamp);
                    LLFIX_UNUSED(success);

                    if(keep_message_records_in_memory)
                    {
                        uint32_t sequence_no = MessageSequenceNumberExtractorType::get_sequence_number_from_message(message);

                        if(sequence_no>0)
                        {
                            MessageMemoryRecord record;
                            record.serialised_file_number = serialised_file_number;
                            record.length = msg_len;
                            record.offset = static_cast<uint64_t>(message_offset);
                            m_message_memory_records.insert({ sequence_no, record });
                        }
                    }
                };

            return deserialise(buffer, buffer_size, message_count, read_bytes, handler);
        }
};

} // namespace