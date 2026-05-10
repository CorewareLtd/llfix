cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(example_order_router LANGUAGES CXX)

include("../../llfix.cmake")

add_executable(example_order_router example.cpp order_router.cpp)

configure_llfix(example_order_router)

configure_file(configs/engine.cfg configs/engine.cfg COPYONLY)
configure_file(configs/inbound.cfg configs/inbound.cfg COPYONLY)
configure_file(configs/outbound.cfg configs/outbound.cfg COPYONLY)
