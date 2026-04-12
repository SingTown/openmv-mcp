cmake_minimum_required(VERSION 3.18)
project(openmv_mcp_server CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Core sources (shared between server and test)
set(CORE_SOURCES
    src/server/mcp_server.cpp
    src/server/mcp_tool.cpp
    src/client/mcp_client.cpp
    src/board.cpp
    src/firmware.cpp
    src/camera.cpp
    src/info.cpp
    src/frame.cpp
    src/protocol/protocol_detect.cpp
    src/protocol/protocol_v1.cpp
    src/protocol/protocol_v2.cpp
    src/serial_port/serial_port.cpp
    src/subprocess/subprocess.cpp
    src/resource.cpp
)

if(APPLE)
    list(APPEND CORE_SOURCES
        src/camera_list/camera_list_darwin.cpp
        src/serial_port/serial_port_unix.cpp
        src/subprocess/subprocess_unix.cpp)
elseif(UNIX)
    list(APPEND CORE_SOURCES
        src/camera_list/camera_list_linux.cpp
        src/serial_port/serial_port_unix.cpp
        src/subprocess/subprocess_unix.cpp)
elseif(WIN32)
    list(APPEND CORE_SOURCES
        src/camera_list/camera_list_win.cpp
        src/serial_port/serial_port_win.cpp
        src/subprocess/subprocess_win.cpp)
endif()

# Common include dirs
set(COMMON_INCLUDES src src/camera_list src/utils 3rdparty 3rdparty/nlohmann-json/single_include)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
set(COMMON_LIBS Threads::Threads OpenSSL::SSL OpenSSL::Crypto)

if(APPLE)
    list(APPEND COMMON_LIBS "-framework IOKit" "-framework CoreFoundation" "-framework Security")
elseif(UNIX)
    list(APPEND COMMON_LIBS udev)
elseif(WIN32)
    list(APPEND COMMON_LIBS setupapi)
endif()

# --- Embedded resource (zip packaged into the binary via CMakeRC) ---
set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resource)
if(NOT EXISTS ${RESOURCE_DIR}/firmware)
    message(FATAL_ERROR
        "resource/firmware not found. Download resources first (see CLAUDE.md 'Download Resources').")
endif()

if(APPLE)
    set(_os_dfu osx)
    set(_os_sdp mac)
    set(_os_blh mac)
    set(_os_stc mac)
    set(_os_mpy mac)
elseif(WIN32)
    set(_os_dfu windows)
    set(_os_sdp win)
    set(_os_blh win)
    set(_os_stc windows)
    set(_os_mpy windows)
elseif(UNIX)
    set(_os_dfu linux64)
    set(_os_sdp linux)
    set(_os_blh linux)
    set(_os_stc linux64)
    set(_os_mpy linux-x86_64)
endif()
set(EMBED_SUBDIRS
    drivers firmware
    dfu-util/${_os_dfu} sdphost/${_os_sdp} blhost/${_os_blh}
    stcubeprogrammer/${_os_stc} mpy-cross/${_os_mpy})

set(EMBED_INPUT_FILES)
set(EMBED_REL_PATHS)
foreach(sub ${EMBED_SUBDIRS})
    if(EXISTS ${RESOURCE_DIR}/${sub})
        file(GLOB_RECURSE _files CONFIGURE_DEPENDS ${RESOURCE_DIR}/${sub}/*)
        list(APPEND EMBED_INPUT_FILES ${_files})
    endif()
endforeach()
foreach(f ${EMBED_INPUT_FILES})
    file(RELATIVE_PATH _rel ${RESOURCE_DIR} ${f})
    list(APPEND EMBED_REL_PATHS ${_rel})
endforeach()

set(EMBED_ZIP ${CMAKE_BINARY_DIR}/embedded_resource.zip)
# Write file list to a response file to avoid command line length limits.
set(EMBED_LIST_FILE ${CMAKE_BINARY_DIR}/embedded_resource_files.txt)
string(REPLACE ";" "\n" _embed_list_content "${EMBED_REL_PATHS}")
file(WRITE ${EMBED_LIST_FILE} "${_embed_list_content}\n")

add_custom_command(
    OUTPUT ${EMBED_ZIP}
    COMMAND ${CMAKE_COMMAND} -E tar c ${EMBED_ZIP} --format=zip --files-from=${EMBED_LIST_FILE}
    WORKING_DIRECTORY ${RESOURCE_DIR}
    DEPENDS ${EMBED_INPUT_FILES} ${EMBED_LIST_FILE}
    COMMENT "Packaging embedded resources -> embedded_resource.zip"
    VERBATIM
)
add_custom_target(embedded_resource_zip DEPENDS ${EMBED_ZIP})

set(EMBED_HASH_HEADER ${CMAKE_BINARY_DIR}/generated/openmv_resource_hash.h)
add_custom_command(
    OUTPUT ${EMBED_HASH_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DZIP=${EMBED_ZIP}
        -DOUT=${EMBED_HASH_HEADER}
        -P ${CMAKE_SOURCE_DIR}/cmake/write_resource_hash.cmake
    DEPENDS ${EMBED_ZIP}
    VERBATIM
)
add_custom_target(embedded_resource_hash DEPENDS ${EMBED_HASH_HEADER})

# miniz (zip extraction at runtime)
set(MINIZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/miniz EXCLUDE_FROM_ALL)

# CMakeRC (embed the zip blob)
include(3rdparty/cmrc/CMakeRC.cmake)
cmrc_add_resource_library(embedded_resources
    NAMESPACE openmv_embed
    WHENCE ${CMAKE_BINARY_DIR}
    ${EMBED_ZIP}
)
add_dependencies(embedded_resources embedded_resource_zip)

# MCP Server executable
add_executable(openmv_mcp_server ${CORE_SOURCES} src/main.cpp)
target_include_directories(openmv_mcp_server PRIVATE ${COMMON_INCLUDES} ${CMAKE_SOURCE_DIR}/3rdparty/miniz ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(openmv_mcp_server PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(openmv_mcp_server PRIVATE ${COMMON_LIBS} embedded_resources miniz)
add_dependencies(openmv_mcp_server embedded_resource_hash)

# Test executable
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

set(TEST_SOURCES
    tests/mcp_server_test.cpp
    tests/mcp_device_test.cpp
    tests/mcp_ws_test.cpp
    tests/frame_test.cpp
    tests/base64_test.cpp
    tests/crc_test.cpp
    tests/ring_buffer_test.cpp
    tests/utf8_buffer_test.cpp
    tests/subprocess_test.cpp
)

option(ENABLE_FIRMWARE_TEST "Enable mcp_firmware_test" OFF)
if(ENABLE_FIRMWARE_TEST)
    list(APPEND TEST_SOURCES tests/mcp_firmware_test.cpp)
endif()

add_executable(openmv_mcp_server_test ${CORE_SOURCES} ${TEST_SOURCES})
target_include_directories(openmv_mcp_server_test PRIVATE ${COMMON_INCLUDES} ${CMAKE_SOURCE_DIR}/3rdparty/miniz ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(openmv_mcp_server_test PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(openmv_mcp_server_test PRIVATE ${COMMON_LIBS} GTest::gtest_main embedded_resources miniz)
add_dependencies(openmv_mcp_server_test embedded_resource_hash)
gtest_discover_tests(openmv_mcp_server_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTIES TIMEOUT 300 ENVIRONMENT "GTEST_FAIL_FAST=1")

# --- clang-format / clang-tidy targets ---
file(GLOB_RECURSE ALL_SOURCES src/*.cpp src/*.h tests/*.cpp)

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCES}
        COMMENT "Running clang-format"
    )
    add_custom_target(check
        COMMAND ${CLANG_FORMAT} --dry-run -Werror ${ALL_SOURCES}
        COMMENT "Checking clang-format"
    )
endif()

option(ENABLE_CLANG_TIDY "Enable clang-tidy during compilation" ON)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy)
    if(CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()
