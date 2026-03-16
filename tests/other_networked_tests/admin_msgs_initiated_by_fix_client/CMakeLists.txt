cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(admin_msgs_initiated_by_fix_client LANGUAGES CXX)

include("../../../llfix.cmake")

add_executable(admin_msgs_initiated_by_fix_client test.cpp)

configure_llfix(admin_msgs_initiated_by_fix_client)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
##############################################################################