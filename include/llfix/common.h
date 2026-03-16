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

#include "core/compiler/checks.h"
#include "core/cpu/architecture_check.h"
#include "core/os/os_check.h"

#define LLFIX_COMMON_H

#ifdef _WIN32

#ifdef FD_SETSIZE
#if FD_SETSIZE == 64
#error "FD_SETSIZE is 64; include llfix/common.h before any Winsock headers."
#endif
#endif

#define FD_SETSIZE 10240    // Since WSAPoll is broken, Windows implementation uses select to emulate epoll.
                            // And this is to increase socket descriptor count that can be monitored by select on Windows
                            // Reference : https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select

#define WIN32_LEAN_AND_MEAN // Helps to avoid unncessary Windows headers

#include <Ws2tcpip.h>       // ws2tcpip.h should be included before any windows.h

#endif