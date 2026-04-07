# OpenMV MCP Server

An [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server for controlling OpenMV cameras over HTTP + JSON-RPC 2.0.

## Build

Requires CMake >= 3.15 and a C++17 compiler.

```bash
git clone --recurse-submodules https://github.com/SingTown/openmv-mcp.git
cd openmv-mcp

# If already cloned without submodules:
# git submodule update --init

cmake -B build
cmake --build build
```

## Run

```bash
./build/openmv_mcp_server              # default port 15257
./build/openmv_mcp_server --port 9000  # custom port
```

## Test

Tests use Google Test, which is fetched automatically during the first build.

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## Code Quality

Requires `clang-format` and `clang-tidy`.

`clang-tidy` is enabled by default during compilation. To disable it:

```bash
cmake -B build -DENABLE_CLANG_TIDY=OFF
```

```bash
cmake --build build --target format        # format all source files
cmake --build build --target check         # check formatting (for CI)
```

## MCP Inspector

Use `mcp-inspector.json` with [MCP Inspector](https://github.com/modelcontextprotocol/inspector) for debugging:

```bash
npx @modelcontextprotocol/inspector --config mcp-inspector.json
```

## MCP Tools

| Tool | Description |
|---|---|
| `list_cameras` | List all connected OpenMV cameras |
| `camera_connect` | Connect to a camera by serial port path |
| `camera_disconnect` | Disconnect from a camera |
| `camera_reset` | Reset (reboot) the camera |
| `camera_info` | Get camera board/sensor/firmware info |
| `run_script` | Execute MicroPython script on camera |
| `stop_script` | Stop currently running script |
| `read_terminal` | Read terminal output (stdout/stderr) |
| `script_running` | Check if a script is running |
| `read_frame` | Capture a frame as base64 JPEG |

## WebSocket Endpoints

Real-time push endpoints for live monitoring:

| Endpoint | Data | Format |
|---|---|---|
| `/ws/script?camera=<path>` | Script running status | JSON |
| `/ws/terminal?camera=<path>` | Terminal output | Text |
| `/ws/frame?camera=<path>` | Frame buffer | Binary JPEG |

## Project Structure

```
src/
  main.cpp                        Entry point
  camera.h/.cpp                   Camera abstraction (factory + callbacks)
  frame.h/.cpp                    Frame data and JPEG conversion
  board.h/.cpp                    Board/sensor database
  server/
    mcp_server.h/.cpp             HTTP/WebSocket server, JSON-RPC dispatch
    mcp_tool.h/.cpp               Tool definitions and call dispatch
    mcp_context.h                 Camera instance management
  protocol/
    protocol_detect.h/.cpp        Protocol version detection
    protocol_v1.h/.cpp            OpenMV Protocol v1 (opcode-based)
    protocol_v2.h/.cpp            OpenMV Protocol v2 (packet-based, CRC)
  serial_port/
    serial_port.h/.cpp            Serial port abstraction
    serial_port_unix.cpp          macOS/Linux implementation
    serial_port_win.cpp           Windows implementation
  camera_list/
    camera_list.h                 Camera discovery interface
    camera_list_darwin.cpp        macOS (IOKit)
    camera_list_linux.cpp         Linux (udev)
    camera_list_win.cpp           Windows (setupapi)
  utils/
    base64.h                      Base64 encoding
    crc.h                         CRC-16/32 (Protocol v2)
    ring_buffer.h                 Circular buffer
    utf8_buffer.h                 UTF-8 streaming buffer
tests/
  mcp_server_test.h/.cpp          Protocol-level tests
  mcp_device_test.h/.cpp          Device integration tests
  mcp_ws_test.h/.cpp              WebSocket integration tests
  frame_test.cpp                  Frame conversion tests
  base64_test.cpp                 Base64 tests
  crc_test.cpp                    CRC tests
  ring_buffer_test.cpp            Ring buffer tests
  utf8_buffer_test.cpp            UTF-8 buffer tests
3rdparty/
  httplib/                        cpp-httplib (git submodule)
  nlohmann-json/                  nlohmann/json (git submodule)
  stb/                            stb (git submodule)
```

## Third-Party Libraries

| Library | Purpose |
|---|---|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | HTTP/WebSocket server |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing |
| [stb](https://github.com/nothings/stb) | JPEG encoding (stb_image_write) |
| [Google Test](https://github.com/google/googletest) | Unit testing (fetched at build time) |

## License

MIT
