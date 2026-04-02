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

## Project Structure

```
src/
  mcp_server.h         MCP Server class declaration
  mcp_server.cpp       MCP protocol handling and tool implementations
  main.cpp             Entry point
tests/
  mcp_server_test.cpp  Protocol-level tests
3rdparty/
  httplib/             cpp-httplib (git submodule)
  nlohmann-json/       nlohmann/json (git submodule)
```

## Third-Party Libraries

| Library | Purpose |
|---|---|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | HTTP server |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing |
| [Google Test](https://github.com/google/googletest) | Unit testing (fetched at build time) |

## License

MIT
