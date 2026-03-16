cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(logon_password_authentication_client LANGUAGES CXX)

include("../../../../llfix.cmake")

add_executable(logon_password_authentication_client client.cpp)

configure_llfix(logon_password_authentication_client)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
configure_file(config_new_password.cfg config_new_password.cfg COPYONLY)
##############################################################################