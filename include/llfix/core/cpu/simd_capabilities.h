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

#include <type_traits>

#if defined(_MSC_VER) // VOLTRON_EXCLUDE
#include <intrin.h>
#endif // VOLTRON_EXCLUDE

namespace llfix
{

namespace CPUIDConstants
{
    // See Intel SW Developer Manual Volume2a for its all functions and constants
    //
    // FUNCTION NUMBERS
    constexpr int CPUID_FUNCTION_BASIC_PROCESSOR_INFO = 1;
    constexpr int CPUID_FUNCTION_EXTENDED_FEATURE_FLAGS = 7;
    // OTHERS
    constexpr int CPUID_REGISTER_COUNT = 4;
    constexpr int CPUID_BASIC_PROCESSOR_INFO_SUPPORTS_OSXSAVE = 27;
    constexpr int CPUID_BASIC_PROCESSOR_INFO_SUPPORTS_AVX = 28;
    constexpr int CPUID_EXTENDED_FEATURE_FLAGS_INFO_SUPPORTS_AVX2 = 5;
    constexpr int CPUID_EXTENDED_FEATURE_FLAGS_INFO_SUPPORTS_AVX512F = 16;
    constexpr int CPUID_EXTENDED_FEATURE_FLAGS_INFO_SUPPORTS_AVX512BW = 30;

    constexpr unsigned long long XCR0_MASK_SSE_AVX = 0x6ULL; // XMM[1] + YMM[2]
    constexpr unsigned long long XCR0_MASK_AVX512 = 0xE6ULL; // XMM[1] + YMM[2] + Opmask[5] + ZMM_Hi256[6] + Hi16_ZMM[7]
}

enum class CPUIDRegisters
{
    EAX,
    EBX,
    ECX,
    EDX
};

class CPUIDQueryResult
{
    public:

        unsigned int* get_registers()
        {
            return registers;
        }

        unsigned int get_register_value(CPUIDRegisters register_index) const
        {
            return registers[static_cast<std::underlying_type<CPUIDRegisters>::type>(register_index)];
        }

        bool is_bit_set(CPUIDRegisters register_index, int bit_index) const
        {
            return get_register_value(register_index) & (1 << bit_index);
        }

    private:
        unsigned int registers[CPUIDConstants::CPUID_REGISTER_COUNT] = {};
};

class CPUID
{
    public:
        static CPUIDQueryResult query(int function_number)
        {
            CPUIDQueryResult cpuid_info;
            auto registers = cpuid_info.get_registers();
            cpuid(function_number, &registers[0], &registers[1], &registers[2], &registers[3]);
            return cpuid_info;
        }
    private:
        static void cpuid(int function, unsigned int* eax, unsigned int* ebx, unsigned int* ecx, unsigned int* edx)
        {
            #ifdef __linux__
            // Not using cpuid.h on Linux as that header doesn`t have include protection
            asm volatile(
                "cpuid"
                : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                : "a" (function), "c" (0)
                );
            #elif _WIN32
            int info[4];
            __cpuid(info, function);
            *eax = info[0];
            *ebx = info[1];
            *ecx = info[2];
            *edx = info[3];
            #endif
        }
};

class SIMDCapabilities
{
    public:

        SIMDCapabilities()
        {
            m_avx = internal_supports_simd_avx();
            m_avx2 = internal_supports_simd_avx2();
            m_avx512f = internal_supports_simd_avx512f();
            m_avx512bw = internal_supports_simd_avx512bw();
        }

        static SIMDCapabilities& instance()
        {
            static SIMDCapabilities singleton;
            return singleton;
        }

        bool supports_simd_avx() const { return m_avx; }
        bool supports_simd_avx2() const { return m_avx2; }
        bool supports_simd_avx512f() const { return m_avx512f; }                    // Foundations
        bool supports_simd_avx512bw() const { return m_avx512bw; }                  // Byte and word
        bool supports_simd_avx512fbw() const { return m_avx512f && m_avx512bw; }    // Foundations & Byte and word

    private:
        // Caching values in case of multi calls
        bool m_avx = false;
        bool m_avx2 = false;
        bool m_avx512f = false;
        bool m_avx512bw = false;

        static unsigned long long xgetbv(unsigned int xcr)
        {
            #ifdef __linux__
            unsigned int eax = 0;
            unsigned int edx = 0;
            asm volatile(
                ".byte 0x0f, 0x01, 0xd0"
                : "=a" (eax), "=d" (edx)
                : "c" (xcr)
                );
            return (static_cast<unsigned long long>(edx) << 32) | eax;
            #elif _WIN32
            return static_cast<unsigned long long>(_xgetbv(xcr));
            #endif
        }

        static bool os_supports_avx_state()
        {
            const auto info = CPUID::query(CPUIDConstants::CPUID_FUNCTION_BASIC_PROCESSOR_INFO);
            if (!info.is_bit_set(CPUIDRegisters::ECX, CPUIDConstants::CPUID_BASIC_PROCESSOR_INFO_SUPPORTS_OSXSAVE))
            {
                return false;
            }

            const auto xcr0 = xgetbv(0);
            return (xcr0 & CPUIDConstants::XCR0_MASK_SSE_AVX) == CPUIDConstants::XCR0_MASK_SSE_AVX;
        }

        static bool os_supports_avx512_state()
        {
            if (!os_supports_avx_state())
            {
                return false;
            }

            const auto xcr0 = xgetbv(0);
            return (xcr0 & CPUIDConstants::XCR0_MASK_AVX512) == CPUIDConstants::XCR0_MASK_AVX512;
        }

        static bool internal_supports_simd_avx()
        {
            auto info = CPUID::query(CPUIDConstants::CPUID_FUNCTION_BASIC_PROCESSOR_INFO);
            return os_supports_avx_state() && info.is_bit_set(CPUIDRegisters::ECX, CPUIDConstants::CPUID_BASIC_PROCESSOR_INFO_SUPPORTS_AVX);
        }

        static bool internal_supports_simd_avx2()
        {
            auto info = CPUID::query(CPUIDConstants::CPUID_FUNCTION_EXTENDED_FEATURE_FLAGS);
            return os_supports_avx_state() && info.is_bit_set(CPUIDRegisters::EBX, CPUIDConstants::CPUID_EXTENDED_FEATURE_FLAGS_INFO_SUPPORTS_AVX2);
        }

        static bool internal_supports_simd_avx512f()
        {
            auto info = CPUID::query(CPUIDConstants::CPUID_FUNCTION_EXTENDED_FEATURE_FLAGS);
            return os_supports_avx512_state() && info.is_bit_set(CPUIDRegisters::EBX, CPUIDConstants::CPUID_EXTENDED_FEATURE_FLAGS_INFO_SUPPORTS_AVX512F);
        }

        static bool internal_supports_simd_avx512bw()
        {
            auto info = CPUID::query(CPUIDConstants::CPUID_FUNCTION_EXTENDED_FEATURE_FLAGS);
            return os_supports_avx512_state() && info.is_bit_set(CPUIDRegisters::EBX, CPUIDConstants::CPUID_EXTENDED_FEATURE_FLAGS_INFO_SUPPORTS_AVX512BW);
        }
};

} // namespace