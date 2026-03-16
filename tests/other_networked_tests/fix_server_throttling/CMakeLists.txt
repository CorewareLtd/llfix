cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(fix_server_throttling LANGUAGES CXX)

include("../../../llfix.cmake")

add_executable(fix_server_throttling test.cpp)

configure_llfix(fix_server_throttling)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
##############################################################################