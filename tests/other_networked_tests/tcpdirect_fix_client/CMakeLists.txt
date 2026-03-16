cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(tcpdirect_fix_client LANGUAGES CXX)

include("../../../llfix.cmake")

add_executable(tcpdirect_fix_client example.cpp)

set(LLFIX_ENABLE_TCPDIRECT ON)

configure_llfix(tcpdirect_fix_client)

##############################################################################
# TEST SPECIFIC OPTIONS ( ON TOP OF llfix.cmake OPTIONS )
option(ENABLE_SOLARFLARE_SHARED_STACK "ENABLE_SOLARFLARE_SHARED_STACK" OFF)

if(ENABLE_SOLARFLARE_SHARED_STACK)
    target_compile_definitions(server PUBLIC ENABLE_SOLARFLARE_SHARED_STACK)
endif()

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
configure_file(test_orders.csv test_orders.csv COPYONLY)
##############################################################################