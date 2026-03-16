cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(example_generated_server LANGUAGES CXX)

include("../../llfix.cmake")

add_executable(example_generated_server server.cpp MyServer.cpp)

set(LLFIX_ENABLE_DICTIONARY ON)
configure_llfix(example_generated_server)

configure_file(config.cfg config.cfg COPYONLY)
configure_file(FIX44_reduced.xml FIX44_reduced.xml COPYONLY)