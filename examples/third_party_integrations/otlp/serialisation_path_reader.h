#pragma once

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include <llfix/core/compiler/builtin_functions.h>
#include <llfix/core/compiler/unused.h>
#include <llfix/core/compiler/packed.h>

#include <llfix/core/os/memory_mapped_file.h>
#include <llfix/core/utilities/serialisation_utilities.h>
#include <llfix/electronic_trading/common/message_serialiser.h>

#include <llfix/fix_utilities.h>

namespace llfix
{

LLFIX_PACKED
(
    struct MessageRecord // 29 BYTES
    {
        int serialised_file_number = 0;
        uint64_t offset=0;
        uint64_t length=0;
        bool successfully_transmitted = false;
        uint64_t timestamp = 0;
    }
);

class SerialisationPathReader
{
    public:

        bool initialise(const std::string& path, std::size_t serialisation_file_max_size)
        {
            assert(!path.empty() && serialisation_file_max_size > 0);

            m_path = path;
            m_serialised_file_max_size = serialisation_file_max_size;

            if (m_serialisation_path.initialise(m_path) == false)
            {
                return false;
            }

            m_records.reserve(1024);
            update_records();

            return true;
        }

        bool has_message(uint64_t message_index) const
        {
            if(m_records.count(message_index) == 0)
                return false;

            return true;
        }

        char* get_message(uint64_t message_index, std::size_t& message_size, bool& successfully_transmitted)
        {
            if(!has_message(message_index))
                return nullptr;

            auto record = m_records[message_index];

            char* buffer = reinterpret_cast<char*>(malloc(static_cast<std::size_t>(record.length)));

            if (!buffer)
                return nullptr;

            if (!get_message_buffer(buffer, record.serialised_file_number, record.offset, record.length))
            {
                free(buffer);
                return nullptr;
            }

            successfully_transmitted = record.successfully_transmitted;
            message_size = static_cast<std::size_t>(record.length);
            return buffer;
        }

        uint64_t get_latest_message_index()
        {
            update_records();
            return m_latest_message_index;
        }

        void update_records()
        {
            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 1. First we need to be up-to-date in the existing file
            if (m_latest_processed_file_number != 0)
            {
                if(m_latest_memory_mapped_file.is_open())
                {
                    char* read_buffer{ nullptr };
                    std::size_t read_buffer_size = 0;
                    m_latest_memory_mapped_file.get_buffer(&read_buffer, read_buffer_size);

                    uint64_t latest_processed_file_last_message_count = *(reinterpret_cast<uint64_t*>(read_buffer));

                    if (latest_processed_file_last_message_count > m_latest_processed_file_last_message_count)
                    {
                        auto read_bytes = process_new_messages_in_latest_serialisation_file(m_latest_processed_file_number, read_buffer, m_latest_processed_file_written_bytes, latest_processed_file_last_message_count - m_latest_processed_file_last_message_count);
                        m_latest_processed_file_written_bytes += read_bytes;
                        m_latest_processed_file_last_message_count = latest_processed_file_last_message_count;
                    }
                }
            }
            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // 2. Now need to check if there are more files in the path
            m_serialisation_path.initialise(m_path);
            auto latest_file_number = m_serialisation_path.get_latest_serialised_file_number();

            if (latest_file_number > m_latest_processed_file_number)
            {
                for (int i = m_latest_processed_file_number + 1; i <= latest_file_number; i++)
                {
                    if(m_latest_memory_mapped_file.is_open())
                    {
                        m_latest_memory_mapped_file.close();
                    }

                    if (m_latest_memory_mapped_file.open(m_serialisation_path.get_serialised_file_path(i), m_serialised_file_max_size))
                    {
                        char* buffer{ nullptr };
                        std::size_t buffer_size = 0;
                        m_latest_memory_mapped_file.get_buffer(&buffer, buffer_size);

                        auto handler = [&](uint64_t timestamp, bool success, uint64_t msg_len, const std::string& message, std::size_t message_offset)
                        {
                            LLFIX_UNUSED(message);
                            add_record(i, timestamp, message_offset, msg_len, success);
                        };

                        llfix::MessageSerialiser<FixMessageSequenceNumberExtractor>::deserialise(buffer, buffer_size, m_latest_processed_file_last_message_count, m_latest_processed_file_written_bytes, handler);

                        m_latest_processed_file_number = i;
                    }
                }
            }
        }

    private:
        SerialisationUtilities::SerialisationFolder m_serialisation_path;
        std::string m_path;
        std::size_t m_serialised_file_max_size = 0;

        std::unordered_map<uint64_t, MessageRecord> m_records;
        uint64_t m_latest_message_index = 0;

        MemoryMappedFile m_latest_memory_mapped_file;
        int m_latest_processed_file_number = 0;
        uint64_t m_latest_processed_file_last_message_count = 0;
        uint64_t m_latest_processed_file_written_bytes = 0;

        uint64_t process_new_messages_in_latest_serialisation_file(int bin_file_number, char* buffer, uint64_t start_offset, uint64_t new_message_count_to_process)
        {
            uint64_t read_message_count = 0;
            uint64_t buffer_offset = start_offset;

            while (read_message_count != new_message_count_to_process)
            {
                uint64_t timestamp = *(reinterpret_cast<uint64_t*>(buffer + buffer_offset));
                buffer_offset += sizeof(uint64_t);

                // SUCCESSFULLY TRANSMITTED
                bool current_message_successfully_transmitted{ false };
                uint8_t value_success = *(reinterpret_cast<uint8_t*>(buffer + buffer_offset));

                assert(value_success == 1 || value_success == 0);

                if (value_success == 1)
                {
                    current_message_successfully_transmitted = true;
                }

                buffer_offset += 1;

                // MESSAGE LENGTH
                uint64_t current_message_length = *(reinterpret_cast<uint64_t*>(buffer + buffer_offset));
                buffer_offset += sizeof(uint64_t);

                // MESSAGE
                add_record(bin_file_number, timestamp, buffer_offset, current_message_length, current_message_successfully_transmitted);

                read_message_count++;

                buffer_offset += current_message_length;
            }

            return buffer_offset-start_offset;
        }

        void add_record(int serialised_file_number, uint64_t timestamp, uint64_t offset, uint64_t length, bool successfully_transmitted)
        {
            MessageRecord record;
            record.serialised_file_number = serialised_file_number;
            record.offset = offset;
            record.length = length;
            record.successfully_transmitted = successfully_transmitted;
            record.timestamp = timestamp;

            m_latest_message_index++;
            m_records[m_latest_message_index] = record;
        }

        bool get_message_buffer(char* buffer, int bin_file_number, uint64_t offset, uint64_t length)
        {
            assert(buffer);

            MemoryMappedFile target_file;

            if (target_file.open(m_serialisation_path.get_serialised_file_path(bin_file_number), m_serialised_file_max_size) == false)
            {
                return false;
            }

            char* read_buffer{ nullptr };
            std::size_t read_buffer_size = 0;

            target_file.get_buffer(&read_buffer, read_buffer_size);

            llfix_builtin_memcpy(buffer, read_buffer + offset, length);

            target_file.close();

            return true;
        }
};

}