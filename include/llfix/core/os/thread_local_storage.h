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
    Standard C++ thread_local keyword does not allow you to specify thread specific destructors
    and also can't be applied to class members
*/
#if __linux__ // VOLTRON_EXCLUDE
#include <pthread.h>
#elif _WIN32 // VOLTRON_EXCLUDE
#include <windows.h>
#include <fibersapi.h>
#endif // VOLTRON_EXCLUDE

namespace llfix
{

class ThreadLocalStorage
{
    public:

        static ThreadLocalStorage& get_instance()
        {
            static ThreadLocalStorage instance;
            return instance;
        }

        // Call it only once for a process
        bool create(void(*thread_destructor)(void*) = nullptr)
        {
            #if __linux__
            if(pthread_key_create(&m_tls_index, thread_destructor) == 0)
                m_initialised = true;
            #elif _WIN32
            // Using FLSs rather TLS as it is identical + TLSAlloc doesn't support dtor
            m_tls_index = FlsAlloc(thread_destructor);
            m_initialised = (m_tls_index == FLS_OUT_OF_INDEXES ? false : true);
            #endif
            return m_initialised;
        }

        // Same as create
        void destroy()
        {
            if (m_initialised)
            {
                #if __linux__
                pthread_key_delete(m_tls_index);
                #elif _WIN32
                FlsFree(m_tls_index);
                #endif
            }
        }

        // GUARANTEED TO BE THREAD-SAFE/LOCAL
        void* get()
        {
            #if __linux__
            return pthread_getspecific(m_tls_index);
            #elif _WIN32
            return FlsGetValue(m_tls_index);
            #endif
        }

        void set(void* data_address)
        {
            #if __linux__
            pthread_setspecific(m_tls_index, data_address);
            #elif _WIN32
            FlsSetValue(m_tls_index, data_address);
            #endif
        }

    private:
        #if __linux__
        pthread_key_t m_tls_index = 0;
        #elif _WIN32
        unsigned long m_tls_index = 0;
        #endif
        bool m_initialised = false;

        ThreadLocalStorage() = default;
        ~ThreadLocalStorage() = default;

        ThreadLocalStorage(const ThreadLocalStorage& other) = delete;
        ThreadLocalStorage& operator= (const ThreadLocalStorage& other) = delete;
        ThreadLocalStorage(ThreadLocalStorage&& other) = delete;
        ThreadLocalStorage& operator=(ThreadLocalStorage&& other) = delete;
};

}