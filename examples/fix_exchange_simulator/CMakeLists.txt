cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(example_fix_exchange_simulator LANGUAGES CXX)

include("../../llfix.cmake")

add_executable(example_fix_exchange_simulator example.cpp exchange_simulator.cpp)

configure_llfix(example_fix_exchange_simulator)

configure_file(config.cfg config.cfg COPYONLY)
