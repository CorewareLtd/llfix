cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(networked_server_rx LANGUAGES CXX)

# Force Release if single-config generator and not specified
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include("../../llfix.cmake")

add_executable(networked_server_rx server.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(networked_server_rx PRIVATE
          $<$<CONFIG:Release>:-march=native -mtune=native>
      )
endif()

configure_llfix(networked_server_rx)

##############################################################################
# TEST SPECIFIC OPTIONS ( ON TOP OF llfix.cmake OPTIONS )
option(ENABLE_SCALABLE_SERVER "ENABLE_SCALABLE_SERVER" OFF)

if(ENABLE_SCALABLE_SERVER)
    target_compile_definitions(networked_server_rx PUBLIC ENABLE_SCALABLE_SERVER)
endif()

option(ENABLE_VALIDATIONS "ENABLE_VALIDATIONS" OFF)

if(ENABLE_VALIDATIONS)
    target_compile_definitions(networked_server_rx PUBLIC ENABLE_VALIDATIONS)
endif()

##############################################################################
# FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
configure_file(deserialiser.exe deserialiser.exe COPYONLY)
configure_file(deserialiser deserialiser COPYONLY)
configure_file(calculate_throughput.py calculate_throughput.py COPYONLY)

set(DICT_SRC "${CMAKE_CURRENT_LIST_DIR}/../../tests/dictionaries")
set(DICT_DST "${CMAKE_CURRENT_BINARY_DIR}/../../tests/dictionaries")

add_custom_command(
TARGET networked_server_rx POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DICT_DST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${DICT_SRC}" "${DICT_DST}"
)
##############################################################################