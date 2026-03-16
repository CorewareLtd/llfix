cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(stress_tests_clients LANGUAGES C CXX)

include("../../../llfix.cmake")

add_executable(stress_tests_clients 
                clients.cpp
)

configure_llfix(stress_tests_clients)

##############################################################################
# TEST SPECIFIC OPTIONS ( ON TOP OF llfix.cmake OPTIONS )
option(ENABLE_MESSAGE_REPLAY "ENABLE_MESSAGE_REPLAY" OFF)

if(ENABLE_MESSAGE_REPLAY)
    target_compile_definitions(stress_tests_clients PUBLIC ENABLE_MESSAGE_REPLAY)
endif()

##############################################################################
# TEST FILES SETUP
configure_file(config_local.cfg config_local.cfg COPYONLY)
configure_file(config_remote.cfg config_remote.cfg COPYONLY)

configure_file(ca.crt ca.crt COPYONLY)
configure_file(client.crt client.crt COPYONLY)
configure_file(client.key client.key COPYONLY)

set(DICT_SRC "${CMAKE_CURRENT_LIST_DIR}/../../dictionaries")
set(DICT_DST "${CMAKE_CURRENT_BINARY_DIR}/../../dictionaries")

add_custom_command(
TARGET stress_tests_clients POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DICT_DST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${DICT_SRC}" "${DICT_DST}"
)
##############################################################################
