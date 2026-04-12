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

#if defined(_MSC_VER)
#include <intrin.h>
#include <cstring>
#endif

//////////////////////////////////////////////////////////////////////
// memcpy
#if defined(__GNUC__)
#define llfix_builtin_memcpy(destination, source, size)     __builtin_memcpy(destination, source, size)
#elif defined(_MSC_VER)
#define llfix_builtin_memcpy(destination, source, size)     std::memcpy(destination, source, size)
#endif

//////////////////////////////////////////////////////////////////////
// memset
#if defined(__GNUC__)
#define llfix_builtin_memset(destination, character, count)  __builtin_memset(destination, character, count)
#elif defined(_MSC_VER)
#define llfix_builtin_memset(destination, character, count)  std::memset(destination, character, count)
#endif

//////////////////////////////////////////////////////////////////////
// Count leading zeroes
namespace llfix
{

#if defined(__GNUC__)
#define llfix_builtin_clzl(n)     __builtin_clzl(n)
#elif defined(_MSC_VER)
#if defined(_WIN64)    // Implementation is for 64-bit only.
inline int llfix_builtin_clzl(unsigned long long value)
{
    unsigned long index = 0;
    return _BitScanReverse64(&index, static_cast<unsigned __int64>(value)) ? static_cast<int>(63 - index) : 64;
}
#else
#error "This code is intended for 64-bit Windows platforms only."
#endif
#endif

#if defined(__GNUC__)
#define llfix_builtin_clz(n)     __builtin_clz(n)
#elif defined(_MSC_VER)
inline int llfix_builtin_clz(unsigned long value) // Implementation is for 32-bit only.
{
    unsigned long index = 0;
    return _BitScanReverse(&index, value) ? static_cast<int>(31 - index) : 32;
}
#endif

} // namepsace