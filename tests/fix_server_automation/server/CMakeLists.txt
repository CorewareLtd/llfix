cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(fix_server_automation LANGUAGES CXX)

include("../../../llfix.cmake")
add_definitions(-DLLFIX_AUTOMATION)

add_executable(fix_server_automation
                automation.cpp
)
##############################################################################
# TEST SPECIFIC OPTIONS ( ON TOP OF llfix.cmake OPTIONS )
option(ENABLE_SCALABLE_SERVER "ENABLE_SCALABLE_SERVER" OFF)

if(ENABLE_SCALABLE_SERVER)
    target_compile_definitions(fix_server_automation PUBLIC ENABLE_SCALABLE_SERVER)
endif()
##############################################################################
set(LLFIX_ENABLE_DICTIONARY ON)
configure_llfix(fix_server_automation)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
configure_file(deserialiser.exe deserialiser.exe COPYONLY)
configure_file(deserialiser deserialiser COPYONLY)

set(DICT_SRC "${CMAKE_CURRENT_LIST_DIR}/../../dictionaries")
set(DICT_DST "${CMAKE_CURRENT_BINARY_DIR}/../../dictionaries")

add_custom_command(
TARGET fix_server_automation POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DICT_DST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${DICT_SRC}" "${DICT_DST}"
)
##############################################################################