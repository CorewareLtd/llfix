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
// - On Linux you need libnuma ( For ex : RHEL -> sudo yum install numactl-devel & Ubuntu -> sudo apt install libnuma-dev ) and -lnuma for GCC

#include <cstddef>

#ifdef __linux__ // VOLTRON_EXCLUDE
#include <numa.h>
#include <sched.h>
#elif _WIN32 // VOLTRON_EXCLUDE
#include <cstdlib>
#endif // VOLTRON_EXCLUDE

#include "../compiler/unused.h"

namespace llfix
{

class NumaUtilities
{
    public:

        static bool bind_to_numa_node(int node)
        {
            bool ret  = false;

            #ifdef __linux__
            if(numa_run_on_node(node) != -1 )
            {
                ret = true;
            }
            #elif _WIN32
            // NOT SUPPORTED ON WINDOWS
            ret = true;
            #endif

            if(get_current_numa_node() != node)
            {
                return false;
            }

            return ret;
        }

        static int get_current_numa_node()
        {
            int numa_node = -1;

            #ifdef __linux__
            if(numa_available() == -1)
                return -1;

            int cpu = sched_getcpu();
            numa_node = numa_node_of_cpu(cpu);
            #elif _WIN32
            // NOT SUPPORTED ON WINDOWS
            #endif

            return numa_node;
        }

        static void* allocate(std::size_t size)
        {
            #ifdef __linux__
            return numa_alloc(size);
            #elif _WIN32
            return malloc(size);
            #endif
        }

        static void deallocate(void* ptr, std::size_t size)
        {
            #ifdef __linux__
            numa_free(ptr, size);
            #elif _WIN32
            LLFIX_UNUSED(size);
            free(ptr);
            #endif
        }
};

} // namespace