cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(fix_server_validations_logons LANGUAGES CXX)

include("../../../llfix.cmake")

add_executable(fix_server_validations_logons test.cpp)

configure_llfix(fix_server_validations_logons)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
##############################################################################