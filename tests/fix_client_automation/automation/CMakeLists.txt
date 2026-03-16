cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(fix_client_automation LANGUAGES CXX)

include("../../../llfix.cmake")
add_definitions(-DLLFIX_AUTOMATION)

add_executable(fix_client_automation
                automation.cpp
)

set(LLFIX_ENABLE_DICTIONARY ON)
configure_llfix(fix_client_automation)

##############################################################################
# TEST FILES SETUP

add_custom_command(TARGET fix_client_automation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:fix_client_automation>"
            "${CMAKE_CURRENT_LIST_DIR}/automation$<TARGET_FILE_SUFFIX:fix_client_automation>"
)

##############################################################################
