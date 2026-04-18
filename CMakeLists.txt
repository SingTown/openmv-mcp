cmake_minimum_required(VERSION 3.18)
project(openmv_mcp_server LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

# Core sources (shared between server and test)
set(CORE_SOURCES
    src/server/mcp_server.cpp
    src/server/mcp_tool.cpp
    src/client/mcp_client.cpp
    src/board.cpp
    src/camera.cpp
    src/info.cpp
    src/frame.cpp
    src/protocol/protocol_detect.cpp
    src/protocol/protocol_v1.cpp
    src/protocol/protocol_v2.cpp
    src/serial_port/serial_port.cpp
    src/daemonize.cpp
)

if(APPLE)
    list(APPEND CORE_SOURCES
        src/camera_list/camera_list_darwin.cpp
        src/serial_port/serial_port_unix.cpp)
elseif(UNIX)
    list(APPEND CORE_SOURCES
        src/camera_list/camera_list_linux.cpp
        src/serial_port/serial_port_unix.cpp)
elseif(WIN32)
    list(APPEND CORE_SOURCES
        src/camera_list/camera_list_win.cpp
        src/serial_port/serial_port_win.cpp)
endif()

# Common include dirs
set(COMMON_INCLUDES src src/camera_list src/utils)
set(THIRDPARTY_INCLUDES 3rdparty 3rdparty/nlohmann-json/single_include)

if(MSVC)
    set(WARNING_FLAGS /W4 /WX)
else()
    set(WARNING_FLAGS -Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC 13/14 emit false positives for vector::insert and memcpy in
        # optimized builds; downgrade only this class to a non-fatal warning.
        list(APPEND WARNING_FLAGS -Wno-error=stringop-overflow)
    endif()
endif()

find_package(Threads REQUIRED)
set(OPENSSL_USE_STATIC_LIBS ON)
if(WIN32)
    set(OPENSSL_MSVC_STATIC_RT ON)
endif()
find_package(OpenSSL REQUIRED)
set(COMMON_LIBS Threads::Threads OpenSSL::SSL OpenSSL::Crypto)

if(APPLE)
    list(APPEND COMMON_LIBS "-framework IOKit" "-framework CoreFoundation" "-framework Security")
elseif(UNIX)
    list(APPEND COMMON_LIBS udev)
elseif(WIN32)
    list(APPEND COMMON_LIBS setupapi cfgmgr32 crypt32 ws2_32 advapi32 user32)
endif()

# spdlog (logging)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_SYSTEM_INCLUDES ON CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/spdlog)

# MCP Server executable
add_executable(openmv_mcp_server ${CORE_SOURCES} src/main.cpp)
target_include_directories(openmv_mcp_server PRIVATE ${COMMON_INCLUDES})
target_include_directories(openmv_mcp_server SYSTEM PRIVATE ${THIRDPARTY_INCLUDES})
target_compile_definitions(openmv_mcp_server PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_options(openmv_mcp_server PRIVATE ${WARNING_FLAGS})
target_link_libraries(openmv_mcp_server PRIVATE ${COMMON_LIBS} spdlog::spdlog)

# Stage the built binary into vscode-extension/tools/mcp-server/<target>/ so
# the VSIX ships it directly (matches process.platform-process.arch at runtime).
if(APPLE)
    set(VSCE_TARGET "darwin-arm64")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(VSCE_TARGET "linux-arm64")
    else()
        set(VSCE_TARGET "linux-x64")
    endif()
elseif(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
        set(VSCE_TARGET "win32-arm64")
    else()
        set(VSCE_TARGET "win32-x64")
    endif()
endif()

add_custom_command(TARGET openmv_mcp_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_SOURCE_DIR}/vscode-extension/tools/mcp-server/${VSCE_TARGET}
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:openmv_mcp_server>
        ${CMAKE_SOURCE_DIR}/vscode-extension/tools/mcp-server/${VSCE_TARGET}/
    COMMENT "Staging openmv_mcp_server -> vscode-extension/tools/mcp-server/${VSCE_TARGET}/"
)

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
    tests/mcp_stream_test.cpp
    tests/frame_test.cpp
    tests/base64_test.cpp
    tests/crc_test.cpp
    tests/ring_buffer_test.cpp
    tests/utf8_buffer_test.cpp
)

add_executable(openmv_mcp_server_test ${CORE_SOURCES} ${TEST_SOURCES})
target_include_directories(openmv_mcp_server_test PRIVATE ${COMMON_INCLUDES})
target_include_directories(openmv_mcp_server_test SYSTEM PRIVATE ${THIRDPARTY_INCLUDES})
target_compile_definitions(openmv_mcp_server_test PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_options(openmv_mcp_server_test PRIVATE ${WARNING_FLAGS})
target_link_libraries(openmv_mcp_server_test PRIVATE ${COMMON_LIBS} GTest::gtest_main spdlog::spdlog)
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
        message(FATAL_ERROR "clang-tidy not found (disable with -DENABLE_CLANG_TIDY=OFF)")
    endif()
endif()
