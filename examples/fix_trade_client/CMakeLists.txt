cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(example_fix_trade_client LANGUAGES CXX)

include("../../llfix.cmake")

add_executable(example_fix_trade_client example.cpp sample_client.cpp)

configure_llfix(example_fix_trade_client)

configure_file(config.cfg config.cfg COPYONLY)
configure_file(test_orders.csv test_orders.csv COPYONLY)
