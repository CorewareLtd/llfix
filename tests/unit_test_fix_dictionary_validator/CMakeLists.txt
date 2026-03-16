cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(unit_test_fix_dictionary_validator LANGUAGES CXX)

include("../../llfix.cmake")
add_definitions(-DLLFIX_UNIT_TEST)
add_definitions(-DLLFIX_ENABLE_BINARY_FIELDS)

add_executable(unit_test_fix_dictionary_validator unit_test.cpp)

set(LLFIX_ENABLE_DICTIONARY ON)
configure_llfix(unit_test_fix_dictionary_validator)

##############################################################################
# TEST FILES SETUP
configure_file(minimal.xml minimal.xml COPYONLY)

set(DICT_SRC "${CMAKE_CURRENT_LIST_DIR}/../dictionaries")
set(DICT_DST "${CMAKE_CURRENT_BINARY_DIR}/../dictionaries")

add_custom_command(
TARGET unit_test_fix_dictionary_validator POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DICT_DST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${DICT_SRC}" "${DICT_DST}"
)
##############################################################################