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

#include <array>
#include <string_view>
#include <cstdio>
#include <type_traits>
#include <mutex>

#if __linux__ // VOLTRON_EXCLUDE
#include <string>
#elif _WIN32 // VOLTRON_EXCLUDE
#include <windows.h>
#endif // VOLTRON_EXCLUDE

namespace llfix
{

enum class ConsoleColour
{
    FG_DEFAULT,
    FG_RED,
    FG_GREEN,
    FG_BLUE,
    FG_YELLOW,
    COUNT
};

class Console
{
    public:
        struct ConsoleColourNode
        {
            ConsoleColour colour;
            int value;
        };

        static void print_colour(ConsoleColour foreground_colour, const std::string_view& buffer)
        {
            const std::lock_guard<std::mutex> lock(m_console_lock);
            auto fg_index = static_cast<std::underlying_type<ConsoleColour>::type>(foreground_colour);

            if (fg_index < 0 || fg_index >= static_cast<std::underlying_type<ConsoleColour>::type>(ConsoleColour::COUNT))
            {
                fg_index = static_cast<std::underlying_type<ConsoleColour>::type>(ConsoleColour::FG_DEFAULT);
            }

            auto foreground_colour_code = NATIVE_CONSOLE_COLOURS[fg_index].value;

            #ifdef _WIN32
            HANDLE handle_console = GetStdHandle(STD_OUTPUT_HANDLE);

            CONSOLE_SCREEN_BUFFER_INFO csbi;
            WORD old_attributes;

            if (GetConsoleScreenBufferInfo(handle_console, &csbi) != 0)
            {
                old_attributes = csbi.wAttributes;
            }
            else
            {
                old_attributes = 15; // default to white on black
            }
            SetConsoleTextAttribute(handle_console, foreground_colour_code | FOREGROUND_INTENSITY);
            fwrite(buffer.data(), 1, buffer.size(), stdout);
            SetConsoleTextAttribute(handle_console, old_attributes);
            #elif __linux__
            std::string ansi_colour_code = "\033[0;" + std::to_string(foreground_colour_code) + "m";
            fprintf(stdout, "%s", ansi_colour_code.c_str());
            fwrite(buffer.data(), 1, buffer.size(), stdout);
            fprintf(stdout, "\033[0m");
            #endif
        }

    private:
        static inline constexpr std::array<ConsoleColourNode, static_cast<std::underlying_type<ConsoleColour>::type>(ConsoleColour::COUNT)> NATIVE_CONSOLE_COLOURS =
        {
            //DO POD INITIALISATION
            {
                #ifdef __linux__
                // https://en.wikipedia.org/wiki/ANSI_escape_code#graphics
                ConsoleColourNode{ConsoleColour::FG_DEFAULT, 0},
                ConsoleColourNode{ConsoleColour::FG_RED, 31},
                ConsoleColourNode{ConsoleColour::FG_GREEN, 32},
                ConsoleColourNode{ConsoleColour::FG_BLUE, 34},
                ConsoleColourNode{ConsoleColour::FG_YELLOW, 33},
                #elif _WIN32
                ConsoleColourNode{ConsoleColour::FG_DEFAULT, 0},
                ConsoleColourNode{ConsoleColour::FG_RED, FOREGROUND_RED},
                ConsoleColourNode{ConsoleColour::FG_GREEN, FOREGROUND_GREEN},
                ConsoleColourNode{ConsoleColour::FG_BLUE, FOREGROUND_BLUE},
                ConsoleColourNode{ConsoleColour::FG_YELLOW, (FOREGROUND_RED | FOREGROUND_GREEN)},
                #endif
            }
        };

        static inline std::mutex m_console_lock;
};

} // namespace