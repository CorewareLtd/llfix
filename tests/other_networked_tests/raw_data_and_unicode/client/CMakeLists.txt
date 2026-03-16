cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(unicode_client LANGUAGES CXX)

include("../../../../llfix.cmake")

add_definitions(-DLLFIX_ENABLE_BINARY_FIELDS)

add_executable(unicode_client client.cpp)

configure_llfix(unicode_client)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
##############################################################################