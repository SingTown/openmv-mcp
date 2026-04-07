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

### HTTP Endpoints

- `POST /mcp` — JSON-RPC 2.0 endpoint (MCP protocol version `2025-03-26`)
- `GET /health` — health check, returns `{"status": "ok"}`

### WebSocket Endpoints (real-time push)

- `GET /ws/script?camera=<path>` — script running status stream (JSON)
- `GET /ws/terminal?camera=<path>` — terminal output stream (text)
- `GET /ws/frame?camera=<path>` — frame buffer stream (binary JPEG)

### Core Modules

- **MCP Server**: `src/server/mcp_server.h/.cpp` — HTTP routing, JSON-RPC dispatch, WebSocket endpoints
- **MCP Tool**: `src/server/mcp_tool.h/.cpp` — tool definitions (`ALL_MCP_TOOLS` vector) and tool call dispatch
- **MCP Context**: `src/server/mcp_context.h` — camera instance management
- **Camera**: `src/camera.h/.cpp` — abstract camera class with factory pattern (`Camera::create()`) and callback system (connected/script/terminal/frame)
- **Frame**: `src/frame.h/.cpp` — frame data with pixel format conversion, JPEG encoding via stb
- **Protocol**: `src/protocol/` — OpenMV protocol v1 (opcode-based) and v2 (packet-based with CRC)
- **Serial Port**: `src/serial_port/` — cross-platform serial I/O (macOS/Linux/Windows)
- **Subprocess**: `src/subprocess/` — cross-platform process execution with streaming output capture
- **Camera List**: `src/camera_list/` — platform-specific USB camera discovery
- **Board**: `src/board.h/.cpp` — board/sensor database and USB device lookup
- **Utilities**: `src/utils/` — base64, CRC, ring buffer, UTF-8 buffer

### Request Flow

`POST /mcp` → `handleRequest()` → method handler (`handleInitialize`, `handleToolsList`, `handleToolsCall`) → tool dispatch via `McpTool::call()`

C++17, header-only third-party libs (`cpp-httplib`, `nlohmann/json`, `stb`) managed as git submodules in `3rdparty/`. Google Test fetched at build time via CMake FetchContent.

## Conventions

- Always respond in Chinese (简体中文)
- Code style enforced by `.clang-format` and `.clang-tidy`
- All source lives under `src/`, tests under `tests/`
- Namespace: `mcp`
