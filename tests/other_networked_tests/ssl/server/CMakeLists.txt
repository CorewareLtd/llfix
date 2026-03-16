cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(ssl_server LANGUAGES C CXX)

include("../../../../llfix.cmake")

add_executable(ssl_server server.cpp)

set(LLFIX_ENABLE_OPENSSL ON)

configure_llfix(ssl_server)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)

set(SSL_FILES_SRC "${CMAKE_CURRENT_LIST_DIR}/../ssl_test_files")
set(SSL_FILES_DST "${CMAKE_CURRENT_BINARY_DIR}/../ssl_test_files")

add_custom_command(
TARGET ssl_server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SSL_FILES_DST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${SSL_FILES_SRC}" "${SSL_FILES_DST}"
)
##############################################################################
