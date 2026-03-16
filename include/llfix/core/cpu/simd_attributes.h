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

#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
    // For a list of attribute values : https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-target-function-attribute-4
    #define LLFIX_SIMD_TARGET_AVX  __attribute__((target("avx")))
    #define LLFIX_SIMD_TARGET_AVX2 __attribute__((target("avx2")))
    #define LLFIX_SIMD_TARGET_AVX512FBW __attribute__((target("avx512f,avx512bw")))
#elif _MSC_VER
    // Not needed on MSVC
    #define LLFIX_SIMD_TARGET_AVX
    #define LLFIX_SIMD_TARGET_AVX2
    #define LLFIX_SIMD_TARGET_AVX512FBW
#endif