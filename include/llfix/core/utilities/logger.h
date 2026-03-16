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
    - Supports 2 modes which can be set via initialise method :

            1. Syncronous logging : logs are written directly to the backend from the caller place
            2. Async logging : logs are written to the memory and a separate thread pushes them to the backend

    - Doesn't support string formatting

    - Doesn't support rotational log files and memory mapped files as backend but they can be passed as a template argument ( LoggerBackendType )

    - If you do : #define LLFIX_ONLY_ERROR_AND_FATAL_LOGS that mode will strip non-error and non-fatal logs from the binary
*/
#include <string>
#include <string_view>
#include <fstream>
#include <thread>
#include <atomic>
#include <new>
#include <cstddef>

#include "../compiler/builtin_functions.h"
#include "../cpu/alignment_constants.h"

#include "../os/vdso.h"
#include "../os/thread_utilities.h"

#include "userspace_spinlock.h"

namespace llfix
{

enum class LogLevel { LEVEL_FATAL =0, LEVEL_ERROR =1, LEVEL_WARNING =2, LEVEL_INFO=3, LEVEL_DEBUG=4};

class LoggerBackendFile
{
    public:

        bool open(const std::string_view& path)
        {
            m_path = path;
            m_file_stream.open(m_path, std::ios_base::app);
            return m_file_stream.is_open();
        }

        void close()
        {
            if (m_file_stream.is_open())
            {
                m_file_stream.close();
            }
        }

        void write(const std::string_view& buffer)
        {
            m_file_stream << buffer;
        }

        void flush()
        {
            m_file_stream << std::flush;
        }

    private:

        std::string m_path;
        std::ofstream m_file_stream;
};

template <typename LoggerBackendType=LoggerBackendFile, bool use_utc_timestamps=false>
class Logger
{
    public:

        static Logger& get_instance()
        {
            static Logger singleton;
            return singleton;
        }

        bool initialise(const std::string_view& path, LogLevel level, bool is_async, std::size_t buffer_capacity=65536)
        {
            if(buffer_capacity < MIN_BUFFER_CAPACITY  || buffer_capacity>MAX_BUFFER_CAPACITY)
            {
                return false;
            }

            if (m_buffer)
            {
                delete[]m_buffer;
                m_buffer = nullptr;
            }

            m_buffer = new (std::nothrow) char[buffer_capacity];

            if (m_buffer == nullptr)
            {
                return false;
            }

            m_buffer_capacity = buffer_capacity;
            m_lock.initialise();

            if(!m_logger_backend.open(path))
            {
                delete[]m_buffer;
                m_buffer = nullptr;
                return false;
            }

            set_log_level(level);

            m_is_async = is_async;

            if(m_is_async)
            {
                m_thread = std::thread(&Logger::thread_append_async_buffer_to_backend, this);
            }

            return true;
        }

        template <LogLevel level>
        void append_log(const std::string_view& message)
        {
            auto loglevel = static_cast<std::underlying_type<LogLevel>::type>(level);

            if (m_log_level >= loglevel)
            {
                m_lock.lock();

                if (m_is_async) // ASYNC MODE , APPEND TO LIVE MEMORY
                {
                    auto str_len = build_log_entry_in_memory<level>(m_buffer + m_buffer_offset, message);
                    m_buffer_offset += str_len;
                }
                else // SYNC MODE , APPEND TO THE BACKEND DIRECTLY
                {
                    auto str_len = build_log_entry_in_memory<level>(m_buffer, message);
                    std::string_view view(m_buffer, str_len);

                    m_logger_backend.write(view);
                    m_logger_backend.flush();
                }

                m_lock.unlock();
            }
        }

        ~Logger()
        {
            m_is_finishing.store(true);

            if (m_thread.joinable())
            {
                m_thread.join();
            }

            if (m_buffer)
            {
                delete[]m_buffer;
                m_buffer = nullptr;
            }

            m_logger_backend.close();
        }

        static LogLevel convert_string_to_log_level(const std::string_view& str)
        {
            LogLevel level = LogLevel::LEVEL_DEBUG;

            if(str == "ERROR")
            {
                level = LogLevel::LEVEL_ERROR;
            }
            else if(str == "INFO")
            {
                level = LogLevel::LEVEL_INFO;
            }
            else if(str == "WARNING")
            {
                level = LogLevel::LEVEL_WARNING;
            }
            else if(str == "FATAL")
            {
                level = LogLevel::LEVEL_FATAL;
            }

            return level;
        }

        void set_log_level(LogLevel level)
        {
            m_log_level = static_cast<std::underlying_type<LogLevel>::type>(level);
        }

        LogLevel get_log_level() const { return static_cast<LogLevel>(m_log_level);  }

    private:
        int m_log_level = 0;
        LoggerBackendType m_logger_backend;
        bool m_is_async = false;
        static constexpr inline std::size_t TIMESTAMP_LENGTH=27; // YYYYMMDD-HH:MM:SS.123456789 27 chars
        static constexpr inline std::size_t MIN_BUFFER_CAPACITY=4096;
        static constexpr inline std::size_t MAX_BUFFER_CAPACITY=1048576;

        UserspaceSpinlock<AlignmentConstants::CPU_CACHE_LINE_SIZE> m_lock;

        std::thread m_thread;
        std::atomic<bool> m_is_finishing = false;

        std::size_t m_buffer_capacity = 0;
        char* m_buffer = nullptr;
        std::size_t m_buffer_offset = 0;

        template <LogLevel level>
        std::size_t build_log_entry_in_memory(char* buffer, const std::string_view& message)
        {
            std::size_t built_length{ 0 };

            auto write_to_buffer = [&](const char* bytes, std::size_t length)
                {
                    std::size_t current_length = length;
                    const std::size_t used = m_buffer_offset + built_length;

                    if (used < m_buffer_capacity)
                    {
                        const std::size_t remaining = m_buffer_capacity - used;

                        if (current_length > remaining)
                        {
                            current_length = remaining;
                        }

                        llfix_builtin_memcpy(buffer + built_length, bytes, current_length);
                        built_length += current_length;
                    }
                };

            // LOG LEVEL
            if constexpr (level == LogLevel::LEVEL_DEBUG)
            {
                write_to_buffer("DEBUG ", 6);
            }
            else if constexpr (level == LogLevel::LEVEL_INFO)
            {
                write_to_buffer("INFO ", 5);
            }
            else if constexpr (level == LogLevel::LEVEL_WARNING)
            {
                write_to_buffer("WARNING ", 8);
            }
            else if constexpr (level == LogLevel::LEVEL_ERROR)
            {
                write_to_buffer("ERROR ", 6);
            }
            else if constexpr (level == LogLevel::LEVEL_FATAL)
            {
                write_to_buffer("FATAL ", 6);
            }

            const std::size_t used = m_buffer_offset + built_length;
            if(m_buffer_capacity>used)
            {
                const std::size_t remaining = m_buffer_capacity - used;
                if(remaining>= TIMESTAMP_LENGTH)
                {
                    VDSO::get_datetime_as_string<use_utc_timestamps>(buffer + built_length);
                    built_length += TIMESTAMP_LENGTH;
                }
            }

            // THE PAYLOAD
            write_to_buffer(" : ", 3);
            write_to_buffer(message.data(), message.length());
            write_to_buffer("\n", 1);

            return built_length;
        }

        void thread_append_async_buffer_to_backend()
        {
            ThreadUtilities::set_thread_name(ThreadUtilities::get_current_thread_id(), "LLFIX_LOG_THRD");

            while (true)
            {
                m_lock.lock();

                if (m_buffer_offset > 0)
                {
                    std::string_view buf(m_buffer, m_buffer_offset);

                    m_logger_backend.write(buf);
                    m_logger_backend.flush();

                    m_buffer_offset = 0;
                }

                m_lock.unlock();

                if (m_is_finishing.load() == true)
                {
                    break;
                }
            }
        }
};

} // namespace

#ifdef LLFIX_ONLY_ERROR_AND_FATAL_LOGS
// THAT MODE WILL STRIP NON-ERROR AND NON-FATAL LOGS FROM THE BINARY
#define LLFIX_LOG_DEBUG(MESSAGE) ;
#define LLFIX_LOG_INFO(MESSAGE) ;
#define LLFIX_LOG_WARNING(MESSAGE) ;
#define LLFIX_LOG_ERROR(MESSAGE) (llfix::Logger<>::get_instance().append_log<llfix::LogLevel::LEVEL_ERROR>((MESSAGE)));
#define LLFIX_LOG_FATAL(MESSAGE) (llfix::Logger<>::get_instance().append_log<llfix::LogLevel::LEVEL_FATAL>((MESSAGE)));
#else
#define LLFIX_LOG_DEBUG(MESSAGE) (llfix::Logger<>::get_instance().append_log<llfix::LogLevel::LEVEL_DEBUG>((MESSAGE)));
#define LLFIX_LOG_INFO(MESSAGE) (llfix::Logger<>::get_instance().append_log<llfix::LogLevel::LEVEL_INFO>((MESSAGE)));
#define LLFIX_LOG_WARNING(MESSAGE) (llfix::Logger<>::get_instance().append_log<llfix::LogLevel::LEVEL_WARNING>((MESSAGE)));
#define LLFIX_LOG_ERROR(MESSAGE) (llfix::Logger<>::get_instance().append_log<llfix::LogLevel::LEVEL_ERROR>((MESSAGE)));
#define LLFIX_LOG_FATAL(MESSAGE) (llfix::Logger<>::get_instance().append_log<llfix::LogLevel::LEVEL_FATAL>((MESSAGE)));
#endif