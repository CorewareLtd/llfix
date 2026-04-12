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
        - Creates a new one or opens an existing one
        - The size can't be changed as it is tied to the virtual memory
        - For using huge pages you need to enable them on the system. See virtual_memory.h for how to do that
*/

#include <string_view>
#include <cstddef>
#include <cstdlib>
#include <limits>

#ifdef __linux__ // VOLTRON_EXCLUDE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#elif _WIN32 // VOLTRON_EXCLUDE
#include <windows.h>
#endif // VOLTRON_EXCLUDE

#include "../compiler/builtin_functions.h"

namespace llfix
{

class MemoryMappedFile
{
public:

    ~MemoryMappedFile()
    {
        if (m_buffer != nullptr)
        {
            close();
        }
    }

    // Creates a new one or opens an existing one ( pass size as zero for opening an existing one)
    bool open(const std::string_view& file_path, std::size_t size, bool shared = true)
    {
        close();

        #ifdef __linux__
        m_file_descriptor = ::open(file_path.data(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

        if (m_file_descriptor ==-1)
        {
            fprintf(stderr, "WARNING : Failed to open memory mapped file: %s\n",  std::strerror(errno));
            return false;
        }

        m_size = size;
        m_original_file_size = static_cast<std::size_t>(lseek(m_file_descriptor, 0, SEEK_END));

        if(m_original_file_size == static_cast<std::size_t>(-1))
        {
            ::close(m_file_descriptor);
            m_file_descriptor = -1;
            fprintf(stderr, "WARNING : Failed to open memory mapped file: %s\n",  std::strerror(errno));
            return false;
        }

        if(m_original_file_size < m_size)
        {
            // WE ARE CREATING A NEW ONE
            // On Linux , mmpapping empty files fail , therefore open method populates bytes
            auto delta = m_size - m_original_file_size;
            char* temp_buffer = static_cast<char*>(malloc(delta));

            if(temp_buffer == nullptr)
            {
                ::close(m_file_descriptor);
                m_file_descriptor = -1;
                return false;
            }

            llfix_builtin_memset(temp_buffer, '\0', delta);
            std::size_t total_bytes_written = 0;
            while (total_bytes_written < delta)
            {
                ssize_t bytes_written = ::write(m_file_descriptor, temp_buffer + total_bytes_written, delta - total_bytes_written);

                if (bytes_written < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }

                    free(temp_buffer);
                    ::close(m_file_descriptor);
                    m_file_descriptor = -1;
                    return false;
                }

                if (bytes_written == 0)
                {
                    free(temp_buffer);
                    ::close(m_file_descriptor);
                    m_file_descriptor = -1;
                    return false;
                }

                total_bytes_written += static_cast<std::size_t>(bytes_written);
            }

            free(temp_buffer);
        }
        else if (m_original_file_size > m_size)
        {
            // WE ARE OPENING AN EXISTING ONE
            m_size = m_original_file_size;
        }

        int flags = shared ? MAP_SHARED : MAP_PRIVATE;

        m_buffer = mmap(nullptr, m_size, PROT_WRITE | PROT_READ, flags, m_file_descriptor, 0);

        if (m_buffer == MAP_FAILED)
        {
            ::close(m_file_descriptor);
            return false;
        }
        #elif _WIN32
        DWORD flags = 0;

        if (shared == true)
        {
            flags = FILE_SHARE_READ | FILE_SHARE_WRITE;
        }

        m_handle_file = CreateFileA(file_path.data(), GENERIC_WRITE | GENERIC_READ, flags, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (m_handle_file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        m_size = size;
        LARGE_INTEGER file_size = {};
        if (!GetFileSizeEx(m_handle_file, &file_size))
        {
            CloseHandle(m_handle_file);
            m_handle_file = nullptr;
            return false;
        }

        if (file_size.QuadPart < 0 || static_cast<unsigned long long>(file_size.QuadPart) > static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)()))
        {
            CloseHandle(m_handle_file);
            m_handle_file = nullptr;
            return false;
        }

        m_original_file_size = static_cast<std::size_t>(file_size.QuadPart);

        if (m_original_file_size < m_size)
        {
            // WE ARE CREATING A NEW ONE
            // We follow Linux implementation
            auto delta = m_size - m_original_file_size;
            char* temp_buffer = static_cast<char*>(malloc(delta));

            if(temp_buffer == nullptr)
            {
                CloseHandle(m_handle_file);
                m_handle_file = nullptr;
                return false;
            }

            llfix_builtin_memset(temp_buffer, '\0', delta);

            std::size_t total_bytes_written = 0;
            while (total_bytes_written < delta)
            {
                DWORD bytes_written = 0;
                const auto remaining = delta - total_bytes_written;
                const DWORD chunk_size = static_cast<DWORD>((remaining > static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())) ? (std::numeric_limits<DWORD>::max)() : remaining);

                if (!WriteFile(m_handle_file, temp_buffer + total_bytes_written, chunk_size, &bytes_written, nullptr))
                {
                    free(temp_buffer);
                    CloseHandle(m_handle_file);
                    m_handle_file = nullptr;
                    return false;
                }

                if (bytes_written == 0)
                {
                    free(temp_buffer);
                    CloseHandle(m_handle_file);
                    m_handle_file = nullptr;
                    return false;
                }

                total_bytes_written += static_cast<std::size_t>(bytes_written);
            }

            free(temp_buffer);
        }
        else if (m_original_file_size > m_size)
        {
            // WE ARE OPENING AN EXISTING ONE
            m_size = m_original_file_size;
        }

        DWORD vm_flags = PAGE_READWRITE;
        const unsigned long long mapping_size = static_cast<unsigned long long>(m_size);
        const DWORD mapping_size_high = static_cast<DWORD>(mapping_size >> 32);
        const DWORD mapping_size_low = static_cast<DWORD>(mapping_size & 0xFFFFFFFFull);

        m_handle_mapping = CreateFileMapping(m_handle_file, nullptr, vm_flags, mapping_size_high, mapping_size_low, nullptr);

        if (m_handle_mapping == nullptr)
        {
            CloseHandle(m_handle_file);
            return false;
        }

        m_buffer = MapViewOfFile(m_handle_mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);

        if (m_buffer == nullptr)
        {
            CloseHandle(m_handle_file);
            CloseHandle(m_handle_mapping);
            return false;
        }

        #endif
        return true;
    }

    void get_buffer(char** buffer, std::size_t& buffer_size)
    {
        *buffer = static_cast<char*>(m_buffer);
        buffer_size = static_cast<std::size_t>(m_size);
    }

    bool flush_to_disc()
    {
        #ifdef __linux__
        return msync(m_buffer, m_size, MS_SYNC) == 0;
        #elif _WIN32
        return FlushViewOfFile(m_buffer, m_size) != 0;
        #endif
    }

    void close()
    {
        #ifdef __linux__
        if (m_buffer)
        {
            munmap(m_buffer, m_size);
            m_buffer = nullptr;
        }

        if (m_file_descriptor != -1)
        {
            ::close(m_file_descriptor);
            m_file_descriptor = -1;
        }
        #elif _WIN32
        if (m_buffer)
        {
            UnmapViewOfFile(m_buffer);
            m_buffer = nullptr;
        }

        if (m_handle_mapping)
        {
            CloseHandle(m_handle_mapping);
            m_handle_mapping = nullptr;
        }

        if (m_handle_file)
        {
            CloseHandle(m_handle_file);
            m_handle_file = nullptr;
        }
        #endif

        m_size = 0;
        m_original_file_size = -1;
    }

    bool is_open() const
    {
        #ifdef __linux__
        return m_file_descriptor != -1;
        #elif _WIN32
        return m_handle_file != nullptr;
        #endif
    }

private:

    #ifdef __linux__
    int m_file_descriptor = -1;
    #elif _WIN32
    HANDLE m_handle_file = nullptr;
    HANDLE m_handle_mapping = nullptr;
    #endif

    std::size_t m_original_file_size = -1;
    std::size_t m_size = 0;
    void* m_buffer = nullptr;
};

} // namespace