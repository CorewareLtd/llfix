cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(unit_test_fix_messages LANGUAGES CXX)

include("../../llfix.cmake")
add_definitions(-DLLFIX_UNIT_TEST)

add_executable(unit_test_fix_messages unit_test.cpp)

configure_llfix(unit_test_fix_messages)

##############################################################################
# TEST FILES SETUP
set(DICT_SRC "${CMAKE_CURRENT_LIST_DIR}/messages_serialisations")
set(DICT_DST "${CMAKE_CURRENT_BINARY_DIR}/messages_serialisations")

add_custom_command(
TARGET unit_test_fix_messages POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DICT_DST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${DICT_SRC}" "${DICT_DST}"
)
##############################################################################