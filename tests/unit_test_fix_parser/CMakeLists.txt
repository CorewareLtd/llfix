cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(unit_test_fix_parser LANGUAGES CXX)

include("../../llfix.cmake")
add_definitions(-DLLFIX_UNIT_TEST)
add_definitions(-DLLFIX_ENABLE_BINARY_FIELDS)

add_executable(unit_test_fix_parser unit_test.cpp)

configure_llfix(unit_test_fix_parser)
