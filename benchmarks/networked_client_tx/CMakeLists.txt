cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(networked_client_tx LANGUAGES CXX)

# Force Release if single-config generator and not specified
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include("../../llfix.cmake")

add_executable(networked_client_tx benchmark.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(networked_client_tx PRIVATE
          $<$<CONFIG:Release>:-march=native -mtune=native>
      )
endif()

configure_llfix(networked_client_tx)
##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
configure_file(../onload_profile.cfg onload_profile.cfg COPYONLY)
##############################################################################
