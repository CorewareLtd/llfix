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
// ANSI coloured output for Linux , message box for Windows

#ifndef NDEBUG // VOLTRON_EXCLUDE
#include <cassert>
#ifdef __linux__ // VOLTRON_EXCLUDE
#include <cstdio>
#elif _WIN32 // VOLTRON_EXCLUDE
#include <windows.h>
#endif // VOLTRON_EXCLUDE
#endif // VOLTRON_EXCLUDE

#ifdef NDEBUG
#define llfix_assert_msg(expr, message) ((void)0)
#else
#ifdef __linux__
#define LLFIX_MAKE_RED(x)    "\033[0;31m" x "\033[0m"
#define LLFIX_MAKE_YELLOW(x) "\033[0;33m" x "\033[0m"
#define llfix_assert_msg(expr, message) \
            do { \
                if (!(expr)) { \
                    fprintf(stderr,  LLFIX_MAKE_RED("Assertion failed : ")  LLFIX_MAKE_YELLOW("%s") "\n", message); \
                    assert(false); \
                } \
            } while (0)
#elif _WIN32
#pragma comment(lib, "user32.lib")
#define llfix_assert_msg(expr, message) \
            do { \
                if (!(expr)) { \
                    MessageBoxA(NULL, message, "Assertion Failed", MB_ICONERROR | MB_OK); \
                    assert(false); \
                } \
            } while (0)
#endif
#endif