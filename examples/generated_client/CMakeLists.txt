cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(example_generated_client LANGUAGES CXX)

include("../../llfix.cmake")

add_executable(example_generated_client client.cpp MyClient.cpp)

set(LLFIX_ENABLE_DICTIONARY ON)
configure_llfix(example_generated_client)

configure_file(config.cfg config.cfg COPYONLY)
configure_file(FIX44_reduced.xml FIX44_reduced.xml COPYONLY)