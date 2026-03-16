cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(networked_server_rx_clients LANGUAGES CXX)

# Force Release if single-config generator and not specified
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include("../../../llfix.cmake")

add_executable(networked_server_rx_clients clients.cpp)

configure_llfix(networked_server_rx_clients)

##############################################################################
# FILES SETUP
configure_file(config_local.cfg config_local.cfg COPYONLY)
##############################################################################