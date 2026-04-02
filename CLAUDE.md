# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# First time: git submodule update --init
cmake -B build
cmake --build build

# Run server (default port 15257)
./build/openmv_mcp_server
./build/openmv_mcp_server --port 9000
```

## Test

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## Code Quality

clang-tidy runs automatically during compilation (`CMAKE_CXX_CLANG_TIDY`). Disable with `-DENABLE_CLANG_TIDY=OFF`.

```bash
cmake --build build --target format   # auto-format with clang-format
cmake --build build --target check    # check formatting (CI)
```

## Debug with MCP Inspector

```bash
npx @modelcontextprotocol/inspector --config mcp-inspector.json
```

## Architecture

This is an MCP (Model Context Protocol) server for controlling OpenMV cameras. It uses **Streamable HTTP** transport (not stdio) over JSON-RPC 2.0.

- **Transport**: HTTP POST on `/mcp` endpoint, health check on `/GET /health`
- **Protocol**: JSON-RPC 2.0, MCP protocol version `2025-03-26`
- **Core class**: `mcp::McpServer` in `src/mcp_server.h/.cpp` — handles HTTP routing, JSON-RPC dispatch, and tool registration
- **Request flow**: `server_.Post("/mcp")` → `handleRequest()` → method-specific handler (`handleInitialize`, `handleToolsList`, `handleToolsCall`)
- **Tool definitions**: returned by `toolDefinitions()` static function in `mcp_server.cpp`; tool dispatch happens in `handleToolsCall()`

C++17, header-only third-party libs (`cpp-httplib`, `nlohmann/json`) managed as git submodules in `3rdparty/`. Google Test fetched at build time via CMake FetchContent.

## Conventions

- Always respond in Chinese (简体中文)
- Code style enforced by `.clang-format` and `.clang-tidy`
- All source lives under `src/`, tests under `tests/`
- Namespace: `mcp`
