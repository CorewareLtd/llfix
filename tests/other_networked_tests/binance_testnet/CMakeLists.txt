cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(binance_testnet LANGUAGES C CXX)

include("../../../llfix.cmake")

add_executable(binance_testnet client.cpp)

set(LLFIX_ENABLE_OPENSSL ON)
set(LLFIX_ENABLE_DICTIONARY ON)

configure_llfix(binance_testnet)

##############################################################################
# TEST FILES SETUP
configure_file(config.cfg config.cfg COPYONLY)
configure_file(ed25519_private.pem ed25519_private.pem COPYONLY)
configure_file(ca-certificates.crt ca-certificates.crt COPYONLY)
configure_file(binance-spot-fix-oe.xml binance-spot-fix-oe.xml COPYONLY)
##############################################################################
