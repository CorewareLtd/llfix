cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(fix_client_validations LANGUAGES CXX)

include("../../../llfix.cmake")

add_executable(fix_client_validations test.cpp)

set(LLFIX_ENABLE_DICTIONARY ON)
configure_llfix(fix_client_validations)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)

set(DICT_SRC "${CMAKE_CURRENT_LIST_DIR}/../../dictionaries")
set(DICT_DST "${CMAKE_CURRENT_BINARY_DIR}/../../dictionaries")

add_custom_command(
TARGET fix_client_validations POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DICT_DST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${DICT_SRC}" "${DICT_DST}"
)
##############################################################################