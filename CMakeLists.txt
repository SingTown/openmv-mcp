cmake_minimum_required(VERSION 3.15)
project(openmv_mcp_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Core sources (shared between server and test)
set(CORE_SOURCES
    src/mcp_server.cpp
    src/board.cpp
    src/camera.cpp
    src/protocol/protocol.cpp
    src/protocol/protocol_v1.cpp
    src/protocol/protocol_v2.cpp
    src/serial_port/serial_port.cpp
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
set(COMMON_INCLUDES src src/camera_list src/utils 3rdparty 3rdparty/nlohmann-json/single_include)

find_package(Threads REQUIRED)
set(COMMON_LIBS Threads::Threads)

if(APPLE)
    list(APPEND COMMON_LIBS "-framework IOKit" "-framework CoreFoundation")
elseif(UNIX)
    list(APPEND COMMON_LIBS udev)
elseif(WIN32)
    list(APPEND COMMON_LIBS setupapi)
endif()

# MCP Server executable
add_executable(openmv_mcp_server ${CORE_SOURCES} src/main.cpp)
target_include_directories(openmv_mcp_server PRIVATE ${COMMON_INCLUDES})
target_link_libraries(openmv_mcp_server PRIVATE ${COMMON_LIBS})

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

add_executable(openmv_mcp_server_test ${CORE_SOURCES}
    tests/mcp_server_test.cpp
    tests/crc_test.cpp
    tests/ring_buffer_test.cpp
    tests/utf8_buffer_test.cpp
)
target_include_directories(openmv_mcp_server_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(openmv_mcp_server_test PRIVATE ${COMMON_LIBS} GTest::gtest_main)
gtest_discover_tests(openmv_mcp_server_test)

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
