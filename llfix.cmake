####################################################################################################################################
#
# - Stand alone helper utility to integrate llfix into projects using CMake
#
# - Minimum required CMake version is 3.8
#
# - llfix.cmake functions should be called after a CMake target is available.
#
# - The main function for projects using llfix is 'configure_llfix'
#
# - Available options for configure_llfix : (set with -D<OPTION>=ON)
#
#       LLFIX_STATIC_LIB            build against llfix static library (Default is header only)
#       LLFIX_ENABLE_TCPDIRECT      build with Solarflare TCPDirect (Linux only)
#       LLFIX_ENABLE_NUMA           build with LibNUMA (Linux only)
#       LLFIX_ENABLE_DICTIONARY     build with Dictionary
#       LLFIX_ENABLE_OPENSSL        build with OpenSSL
#
# Note for opensource edition: Only applicable option is LLFIX_ENABLE_NUMA

####################################################################################################################################
# INCLUDE GUARD
get_property(_llfix_cmake_included GLOBAL PROPERTY LLFIX_CMAKE_INCLUDED)
if(NOT _llfix_cmake_included)
    set_property(GLOBAL PROPERTY LLFIX_CMAKE_INCLUDED TRUE)

####################################################################################################################################
# OS CHECK
if(NOT (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    message(FATAL_ERROR "Only Linux and Windows are supported.")
endif()

####################################################################################################################################
# COMPILER CHECK
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Only GCC and Clang are supported on Linux.")
    endif()
endif()

if (WIN32)
    if (NOT MSVC)
        message(FATAL_ERROR "Only MSVC supported on Windows.")
    endif()
endif()

####################################################################################################################################
# ROOT DIR
if(NOT DEFINED LLFIX_ROOT_DIR)
    set(LLFIX_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Path to llfix root directory")
endif()

####################################################################################################################################
# INCLUDE HELPERS
function(configure_llfix_includes_for_target TARGET)
    target_include_directories(${TARGET} PUBLIC "${LLFIX_ROOT_DIR}/include")
endfunction()

function(configure_llfix_static_includes_for_target TARGET)
    target_include_directories(${TARGET} PUBLIC "${LLFIX_ROOT_DIR}/lib/include")
endfunction()

####################################################################################################################################
# LINKER HELPERS
function(configure_pthread_for_target TARGET)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Threads REQUIRED)
        target_link_libraries(${TARGET} PRIVATE Threads::Threads)
    endif()
endfunction()

function(configure_lto_for_target TARGET)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:/GL>")
        target_link_options(${TARGET} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:/LTCG>")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-flto>")
        target_link_options(${TARGET} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-flto>")
    endif()
endfunction()

####################################################################################################################################
# OPTION : SOLARFLARE TCPDIRECT
option(LLFIX_ENABLE_TCPDIRECT "Build with Solarflare TCPDirect" OFF)

function(configure_tcpdirect_for_target TARGET)
    if(LLFIX_ENABLE_TCPDIRECT)
        # OS CHECK
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
            message(FATAL_ERROR "TCPDirect is only supported on Linux.")
        endif()

        # HEADER CHECKS
        find_path(TCPDIRECT_INCLUDE_DIR zf/zf.h)

        if(NOT TCPDIRECT_INCLUDE_DIR)
            message(FATAL_ERROR "zf/zf.h not found (TCPDIRECT_INCLUDE_DIR).")
        else()
            message(STATUS "Found TCPDirect include dir: ${TCPDIRECT_INCLUDE_DIR}")
        endif()

        # STATIC LIB CHECK
        find_library(TCPDIRECT_LIB onload_zf_static)

        if(NOT TCPDIRECT_LIB)
            message(FATAL_ERROR "onload_zf_static not found (TCPDIRECT_LIB).")
        else()
            message(STATUS "Found TCPDirect library: ${TCPDIRECT_LIB}")
        endif()

        # INCLUDES
        target_include_directories(${TARGET} PUBLIC ${TCPDIRECT_INCLUDE_DIR})

        # DEFINES
        target_compile_definitions(${TARGET} PUBLIC LLFIX_ENABLE_TCPDIRECT)

        # LINKER
        target_link_libraries(${TARGET} PRIVATE ${TCPDIRECT_LIB})
    endif()
endfunction()

####################################################################################################################################
# OPTION : LIBNUMA
option(LLFIX_ENABLE_NUMA "Build with LibNUMA" OFF)

function(configure_libnuma_for_target TARGET)
    if(LLFIX_ENABLE_NUMA)
        # OS CHECK
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
            message(FATAL_ERROR "LibNUMA is only supported on Linux.")
        endif()

        # HEADER CHECKS
        find_path(LIBNUMA_INCLUDE_DIR numa.h)

        if(NOT LIBNUMA_INCLUDE_DIR)
            message(FATAL_ERROR "numa.h not found (LIBNUMA_INCLUDE_DIR).")
        else()
            message(STATUS "Found LibNUMA include dir: ${LIBNUMA_INCLUDE_DIR}")
        endif()

        # SHARED OBJECT CHECK
        find_library(LIBNUMA_SO numa)

        if(NOT LIBNUMA_SO)
            message(FATAL_ERROR "libnuma not found (LIBNUMA_SO).")
        else()
            message(STATUS "Found LibNUMA library: ${LIBNUMA_SO}")
        endif()

        # INCLUDES
        target_include_directories(${TARGET} PUBLIC ${LIBNUMA_INCLUDE_DIR})

        # DEFINES
        target_compile_definitions(${TARGET} PUBLIC LLFIX_ENABLE_NUMA)

        # LINKER
        target_link_libraries(${TARGET} PRIVATE ${LIBNUMA_SO})
    endif()
endfunction()

####################################################################################################################################
# OPTION : DICTIONARY
option(LLFIX_ENABLE_DICTIONARY "Build with Dictionary" OFF)

function(configure_dictionary_for_target TARGET)
    if(LLFIX_ENABLE_DICTIONARY)
        # HEADER CHECKS
        if(NOT DEFINED TINYXML_INCLUDE_DIR)
            set(TINYXML_INCLUDE_DIR "${LLFIX_ROOT_DIR}/deps/tinyxml2/include" CACHE PATH "TinyXML2 include directory")
        endif()
        if(NOT DEFINED TINYXML_LIB_DIR)
            set(TINYXML_LIB_DIR "${LLFIX_ROOT_DIR}/deps/tinyxml2/lib" CACHE PATH "TinyXML2 library directory")
        endif()

        target_include_directories(${TARGET} PUBLIC ${TINYXML_INCLUDE_DIR})

        find_file(TINYXML_HEADER tinyxml2.h
            PATHS ${TINYXML_INCLUDE_DIR}
            NO_DEFAULT_PATH
            REQUIRED
        )
        message(STATUS "Found TinyXML2 header: ${TINYXML_HEADER}")

        # STATIC LIB CHECK
        if(WIN32)
            find_library(TINYXML_LIB_RELEASE tinyxml2
                PATHS ${TINYXML_LIB_DIR}
                NO_DEFAULT_PATH
                REQUIRED
            )
            find_library(TINYXML_LIB_DEBUG tinyxml2d
                PATHS ${TINYXML_LIB_DIR}
                NO_DEFAULT_PATH
                REQUIRED
            )
            message(STATUS "Found TinyXML2 libraries: ${TINYXML_LIB_RELEASE} (release), ${TINYXML_LIB_DEBUG} (debug)")
        else()
            find_library(TINYXML_LIB tinyxml2
                PATHS ${TINYXML_LIB_DIR}
                NO_DEFAULT_PATH
                REQUIRED
            )
            message(STATUS "Found TinyXML2 library: ${TINYXML_LIB}")
        endif()

        # DEFINES
        target_compile_definitions(${TARGET} PUBLIC LLFIX_ENABLE_DICTIONARY)

        # LINKER
        if(WIN32)
            target_link_libraries(${TARGET} PRIVATE optimized ${TINYXML_LIB_RELEASE} debug ${TINYXML_LIB_DEBUG})
        else()
            target_link_libraries(${TARGET} PRIVATE ${TINYXML_LIB})
        endif()
    endif()
endfunction()

####################################################################################################################################
# OPTION : OPENSSL
option(LLFIX_ENABLE_OPENSSL "Build with OpenSSL" OFF)

# Note: set ENV{OPENSSL_ROOT_DIR} to override system discovery.
function(configure_openssl_for_target TARGET)
    if(LLFIX_ENABLE_OPENSSL)

        if(WIN32)
                set(SSL_NAME libssl)
                set(CRYPTO_NAME libcrypto)
        else()
                set(SSL_NAME ssl)
                set(CRYPTO_NAME crypto)
        endif()

        if(DEFINED ENV{OPENSSL_ROOT_DIR})
            set(OPENSSL_ROOT_DIR "$ENV{OPENSSL_ROOT_DIR}" CACHE PATH "OpenSSL root directory")

            set(OPENSSL_ROOT ${OPENSSL_ROOT_DIR})
            set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT}/include)

            target_include_directories(${TARGET} PUBLIC ${OPENSSL_INCLUDE_DIR})

            find_file(OPENSSL_HEADER openssl/ssl.h
            PATHS ${OPENSSL_INCLUDE_DIR}
            NO_DEFAULT_PATH
            REQUIRED
            )
            message(STATUS "Found OpenSSL header: ${OPENSSL_HEADER}")

            set(OPENSSL_LIB_DIR ${OPENSSL_ROOT}/lib)

            find_library(SSL_LIB ${SSL_NAME}
                PATHS ${OPENSSL_LIB_DIR}
                NO_DEFAULT_PATH
                REQUIRED
            )

            find_library(CRYPTO_LIB ${CRYPTO_NAME}
                PATHS ${OPENSSL_LIB_DIR}
                NO_DEFAULT_PATH
                REQUIRED
            )

            message(STATUS "Found OpenSSL libraries: ${SSL_LIB}, ${CRYPTO_LIB}")

            # LINKER
            target_link_libraries(${TARGET} PRIVATE ${SSL_LIB} ${CRYPTO_LIB})
        else()
            # System include
            find_path(OPENSSL_INCLUDE_DIR openssl/ssl.h)
            if(NOT OPENSSL_INCLUDE_DIR)
                message(FATAL_ERROR "openssl/ssl.h not found (OPENSSL_INCLUDE_DIR).")
            else()
                message(STATUS "Found OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
            endif()
            target_include_directories(${TARGET} PUBLIC ${OPENSSL_INCLUDE_DIR})

            # System libraries
            find_library(SSL_LIB ${SSL_NAME} REQUIRED )
            find_library(CRYPTO_LIB ${CRYPTO_NAME} REQUIRED )
            message(STATUS "Found OpenSSL libraries: ${SSL_LIB}, ${CRYPTO_LIB}")
            target_link_libraries(${TARGET} PRIVATE ${SSL_LIB} ${CRYPTO_LIB})
        endif()

        # DEFINES
        target_compile_definitions(${TARGET} PUBLIC LLFIX_ENABLE_OPENSSL)

    endif()
endfunction()

function(configure_openssl_applink_for_windows_target TARGET)
    if(LLFIX_ENABLE_OPENSSL)
        if(WIN32)
            if(DEFINED ENV{OPENSSL_ROOT_DIR})
                set(OPENSSL_ROOT_DIR "$ENV{OPENSSL_ROOT_DIR}" CACHE PATH "OpenSSL root directory")
                if(NOT EXISTS "${OPENSSL_ROOT_DIR}/ms/applink.c")
                        message(FATAL_ERROR "OpenSSL applink not found at: ${OPENSSL_ROOT_DIR}/ms/applink.c")
                endif()
                message(STATUS "Found OpenSSL applink: ${OPENSSL_ROOT_DIR}/ms/applink.c")
                target_sources(${TARGET} PRIVATE "${OPENSSL_ROOT_DIR}/ms/applink.c")
            endif()
        endif()
    endif()
endfunction()

####################################################################################################################################
# OPTION : STATIC LIB
option(LLFIX_STATIC_LIB "Build against llfix static library" OFF)

####################################################################################################################################
# COMPOSITE BUILD FUNCTIONS
function(configure_llfix_static TARGET)
    configure_llfix_static_includes_for_target(${TARGET})
    if(NOT TARGET llfix_static)
        add_subdirectory("${LLFIX_ROOT_DIR}/lib" llfix_static_build)
    endif()
    target_link_libraries(${TARGET} PRIVATE llfix_static)
    configure_lto_for_target(${TARGET})
endfunction()

function(configure_llfix_header_only TARGET)
    configure_llfix_includes_for_target(${TARGET})
    configure_pthread_for_target(${TARGET})
    configure_tcpdirect_for_target(${TARGET})
    configure_dictionary_for_target(${TARGET})
    configure_openssl_for_target(${TARGET})
    configure_libnuma_for_target(${TARGET})
endfunction()

function(configure_llfix TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "You must define a target before calling configure_llfix")
    endif()
    if(LLFIX_STATIC_LIB)
        configure_llfix_static(${TARGET})
    else()
        configure_llfix_header_only(${TARGET})
    endif()
    configure_openssl_applink_for_windows_target(${TARGET})
endfunction()

endif()
