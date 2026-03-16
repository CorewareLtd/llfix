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

#include <cstddef>
#include <cstdlib>

#include "../compiler/unused.h"

#ifdef LLFIX_ENABLE_NUMA // VOLTRON_EXCLUDE
#include "../os/numa_utilities.h"
#endif // VOLTRON_EXCLUDE

namespace llfix
{

class Allocator
{
    public:
        static void set_numa_aware(bool b)
        {
            numa_aware = b;
        }

        static void* allocate(std::size_t size)
        {
            #ifdef LLFIX_ENABLE_NUMA
            if(numa_aware)
                return NumaUtilities::allocate(size);
            else
                return malloc(size);
            #else
            return malloc(size);
            #endif
        }

        static void deallocate(void* ptr, std::size_t size)
        {
            #ifdef LLFIX_ENABLE_NUMA
            if(numa_aware)
                NumaUtilities::deallocate(ptr, size);
            else
                free(ptr);
            #else
            LLFIX_UNUSED(size);
            free(ptr);
            #endif
        }
    private:
        static inline bool numa_aware = false;
};

} // namespace