cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(logon_password_authentication_server LANGUAGES CXX)

include("../../../../llfix.cmake")

add_executable(logon_password_authentication_server server.cpp)

configure_llfix(logon_password_authentication_server)

##############################################################################
# TEST FILES SETUP
configure_file(server_config.cfg server_config.cfg COPYONLY)
##############################################################################