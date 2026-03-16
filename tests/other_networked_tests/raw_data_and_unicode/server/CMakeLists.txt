cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(unicode_server LANGUAGES CXX)

include("../../../../llfix.cmake")

add_definitions(-DLLFIX_ENABLE_BINARY_FIELDS)

add_executable(unicode_server server.cpp)

configure_llfix(unicode_server)

##############################################################################
# TEST FILES SETUP
configure_file(server_config.cfg server_config.cfg COPYONLY)
##############################################################################