cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(networked_client_tx_server LANGUAGES CXX)

# Force Release if single-config generator and not specified
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include("../../../llfix.cmake")

add_executable(networked_client_tx_server server.cpp)

configure_llfix(networked_client_tx_server)