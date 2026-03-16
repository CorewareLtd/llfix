/*
    USAGE :

        #include "unit_test.h"

        ...

        UnitTest unit_test;

        unit_test.test_equals(ACTUAL, EXPECTED, "test category", "test case");

        ...

        cout << unit_test.get_summary_report("Tests for X");

        return unit_test.did_all_pass();

    OTHERS :

        ConcurrencyTestUtilities                            sleep_randomly_usecs
        RandomNumberGenerator::get_random_positive_integer  use it to get random positive integers
        Console::print_colour                               gives coloured output in consoles on Linux and Windows
*/
#pragma once

#include <cmath>
#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <random>
#include <type_traits>
#include <cstdio>
#include <mutex>

#if __linux__
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#include <chrono>
#include <thread>
#endif

#ifndef UNIT_TEST
#define UNIT_TEST
#endif

///////////////////////////////////////////////////////////////////
// Console
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
                ConsoleColour::FG_DEFAULT, 0,
                ConsoleColour::FG_RED, 31,
                ConsoleColour::FG_GREEN, 32,
                ConsoleColour::FG_BLUE, 34,
                ConsoleColour::FG_YELLOW, 33,
                #elif _WIN32
                ConsoleColour::FG_DEFAULT, 0,
                ConsoleColour::FG_RED, FOREGROUND_RED,
                ConsoleColour::FG_GREEN, FOREGROUND_GREEN,
                ConsoleColour::FG_BLUE, FOREGROUND_BLUE,
                ConsoleColour::FG_YELLOW, (FOREGROUND_RED | FOREGROUND_GREEN),
                #endif
            }
        };
        
        static inline std::mutex m_console_lock;
};

///////////////////////////////////////////////////////////////////
// RandomNumberGenerator
class RandomNumberGenerator
{
    public:
        static int get_random_positive_integer(int max_random_number=100)
        {
            static thread_local std::default_random_engine rng{std::random_device{}()};
            return std::uniform_int_distribution<int>(1, max_random_number)(rng);
        }
};

///////////////////////////////////////////////////////////////////
// ConcurrencyTestUtilities
class ConcurrencyTestUtilities
{
    public:

        static void sleep_randomly_usecs(int max_duration_in_microsecs=0)
        {
            unsigned long microseconds = static_cast<unsigned long>(RandomNumberGenerator::get_random_positive_integer(max_duration_in_microsecs));
            #ifdef __linux__
            usleep(microseconds);
            #elif _WIN32
            std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
            #endif
        }
};

///////////////////////////////////////////////////////////////////
// UnitTest
class UnitTest
{
    public:

        struct UnitTestResult
        {
            std::string test_category = "";
            std::string test_case = "";
            std::string actual_text = "";
            std::string expected_text = "";
            bool success = false;

            std::string get_as_text() const
            {
                std::stringstream str;
                str << "Category=" << test_category << " , Case=" << test_case << " , Actual=" << actual_text << " , Expected=" << expected_text;
                return str.str();
            }
        };

        template <class T, class U>
        bool test_equals(T actual, U expected, const std::string& test_category, const std::string& test_case)
        {
            bool evaluation = compare(actual, expected);
            process_result(evaluation, actual, expected, test_category, test_case);
            return evaluation;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////////////////
        // COMPARISON METHODS
        template <class T, class U>
        bool compare(T actual, U expected)
        {
            bool evaluation = (actual == static_cast<T>(expected));
            return evaluation;
        }

        bool compare(double actual, double expected)
        {
            const double diff = std::fabs(actual - expected);
            const double scale = std::fmax(1.0, std::fmax(std::fabs(actual), std::fabs(expected)));
            return diff <= 1e-9 * scale;
        }

        bool compare(const char* actual, const char* expected)
        {
            bool evaluation = false;

            if(actual == nullptr && expected == nullptr)
                return true;

            if (actual && expected)
            {
                evaluation = strcmp(actual, expected) == 0;
            }

            return evaluation;
        }
        /////////////////////////////////////////////////////////////////////////////////////////////////////////

        void reset()
        {
            m_results.clear();
        }

        bool did_all_pass() const
        {
            for (const auto& iter : m_results)
            {
                if (iter.success == false)
                {
                    return false;
                }
            }
            return true;
        }

        std::string get_summary_report(const std::string& report_name) const
        {
            std::stringstream result;

            // FIND OUT THE FAILED ONES
            std::vector<UnitTestResult> failed_test_cases;

            for (const auto& iter : m_results)
            {
                if (iter.success == false)
                {
                    failed_test_cases.push_back(iter);
                }
            }

            // BUILD THE REPORT
            auto test_case_number = m_results.size();
            auto failure_number = failed_test_cases.size();

            result << report_name << " > Total test case number : " << test_case_number << " , Failed test case number : " << failure_number;
            result << std::endl << std::endl;

            if (failure_number > 0)
            {
                result << "Failed test cases :" << std::endl ;
                auto counter = 1;

                for (const auto& iter : failed_test_cases)
                {
                    result << counter << ". " << iter.get_as_text() << std::endl;
                    counter++;
                }
            }

            return result.str();
        }

    private:
        std::vector<UnitTestResult> m_results ={};

        template <class T, class U>
        void process_result(bool evaluation_result, T actual, U expected, const std::string& test_category, const std::string& test_case)
        {
            Console::print_colour(ConsoleColour::FG_YELLOW, "RUNNING: Category = " + test_category + " , Test case = " + test_case + '\n');

            std::stringstream stream_actual;
            stream_actual << actual;
            std::stringstream stream_expected;
            stream_expected << expected;
            m_results.push_back({ test_category, test_case, stream_actual.str(), stream_expected.str(), evaluation_result });

            std::stringstream test_case_display;
            test_case_display << "actual=" << actual << " expected=" << expected;

            if (evaluation_result)
            {
                Console::print_colour(ConsoleColour::FG_GREEN, "SUCCESS : " + test_case_display.str() + '\n');
            }
            else
            {
                Console::print_colour(ConsoleColour::FG_RED, "FAILURE : " + test_case_display.str() + '\n');
            }
        }
};