# OpenMV MCP Server

An [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server for controlling OpenMV cameras over HTTP + JSON-RPC 2.0.

## Install

**Linux** (x86_64 / arm64):

```bash
curl -fsSL https://raw.githubusercontent.com/SingTown/openmv-mcp/main/install.sh | sh
```

Pin a version or pick a different install dir:

```bash
curl -fsSL https://raw.githubusercontent.com/SingTown/openmv-mcp/main/install.sh | sh -s -- --version 2.3.3
curl -fsSL https://raw.githubusercontent.com/SingTown/openmv-mcp/main/install.sh | sh -s -- --dir ~/.local/bin
```

**macOS** (Apple Silicon):

```bash
brew install SingTown/openmv/openmv-mcp
```

**Windows**:

```powershell
winget install SingTown.openmv-mcp
```

Or grab a prebuilt binary from the [Releases](https://github.com/SingTown/openmv-mcp/releases) page.

## Run

```bash
openmv_mcp_server             # ensure the HTTP server is running in the background
openmv_mcp_server --port 9000 # ensure a background server on a custom port
openmv_mcp_server --mode stdio # stdio MCP proxy; starts the HTTP server if needed
openmv_mcp_server --level trace
openmv_mcp_server --version   # print version and exit
```

### Background Server

The CLI starts the HTTP server in the background automatically and exits once it is ready. Logs are written to `./openmv-mcp-server-log.txt` by default:

```bash
openmv_mcp_server
openmv_mcp_server --log /tmp/openmv.log  # custom log path
```

Stop a running server:

```bash
openmv_mcp_server --mode shutdown              # default port 15257
openmv_mcp_server --mode shutdown --port 9000  # custom port
```

Flags: `--mode <mode>` (`shutdown`, `stdio`, or `internal_server`), `--log <path>` (write HTTP server logs to a file, default `./openmv-mcp-server-log.txt`), `--level <lvl>` (log level: `trace|debug|info|warn|error|critical|off`, default `info`).

### Stdio mode

Use stdio mode for MCP hosts that launch local command-based servers instead of connecting to Streamable HTTP:

```bash
openmv_mcp_server --mode stdio
```

The stdio process keeps stdin/stdout reserved for MCP JSON-RPC messages and forwards stdio requests to `POST /mcp`. It starts the HTTP server automatically if needed. Pass `--port <port>` to use a non-default server port, and `--log <path>` to override the default HTTP server log path.

## MCP Inspector

Use `mcp-inspector.json` with [MCP Inspector](https://github.com/modelcontextprotocol/inspector) for debugging:

```bash
npx @modelcontextprotocol/inspector --config mcp-inspector.json
```
## MCP Tools

| Tool | Description |
|---|---|
| `camera_list` | List OpenMV cameras with their connection state |
| `camera_connect` | Connect to a camera by serial port path |
| `camera_disconnect` | Disconnect from a camera |
| `camera_reset` | Reset (reboot) the camera |
| `camera_boot` | Reboot the camera into bootloader mode (for firmware flashing) |
| `camera_info` | Get camera board/sensor/firmware info |
| `script_run` | Execute MicroPython script on camera |
| `script_stop` | Stop currently running script |
| `script_running` | Check if a script is running |
| `script_output` | Read script output (stdout/stderr) |
| `script_save` | Save a MicroPython script to main.py on the camera's USB drive |
| `frame_capture` | Capture a frame as base64 JPEG |
| `frame_enable` | Enable or disable frame streaming from the camera |
| `license_register` | Register a board key for the connected camera |

## HTTP Streaming Endpoints

| Endpoint | Data | Format |
|---|---|---|
| `GET /stream/status?camera=<path>` | Connection & script running status | `text/event-stream` (SSE) |
| `GET /stream/frame?camera=<path>` | Frame buffer | `multipart/x-mixed-replace; boundary=frame` (MJPEG) |
| `GET /stream/terminal?camera=<path>` | Terminal output | `text/plain; charset=utf-8` (chunked) |


## License

MIT
