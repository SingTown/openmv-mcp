# OpenMV MCP Server

An [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server for controlling OpenMV cameras over HTTP + JSON-RPC 2.0.

## Run

```bash
openmv_mcp_server              # default port 15257
openmv_mcp_server --port 9000    # custom port
openmv_mcp_server --level trace  # verbose logging
openmv_mcp_server --version      # print version and exit
```

### Daemon mode

Run detached from the terminal so the server keeps running after the shell closes:

```bash
openmv_mcp_server --daemon --log /tmp/openmv.log
```

Stop a running server:

```bash
# Linux / macOS
kill $(lsof -i :15257 -t)

# Windows
for /f "tokens=5" %a in ('netstat -ano ^| findstr :15257 ^| findstr LISTENING') do taskkill /PID %a /F
```

Flags: `--daemon, -d` (fork to background), `--log <path>` (redirect stdout/stderr; defaults to `/dev/null` or `NUL`), `--level <lvl>` (log level: `trace|debug|info|warn|error|critical|off`, default `info`).

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
| `camera_info` | Get camera board/sensor/firmware info |
| `script_run` | Execute MicroPython script on camera |
| `script_stop` | Stop currently running script |
| `script_running` | Check if a script is running |
| `script_output` | Read script output (stdout/stderr) |
| `script_save` | Save a MicroPython script to main.py on the camera's USB drive |
| `frame_capture` | Capture a frame as base64 JPEG |
| `frame_enable` | Enable or disable frame streaming from the camera |
| `firmware_flash` | Flash firmware to the camera |
| `firmware_repair` | Fully repair camera (bootloader + firmware) |
| `license_register` | Register a board key for the connected camera |

## HTTP Streaming Endpoints

| Endpoint | Data | Format |
|---|---|---|
| `GET /stream/status?camera=<path>` | Connection & script running status | `text/event-stream` (SSE) |
| `GET /stream/frame?camera=<path>` | Frame buffer | `multipart/x-mixed-replace; boundary=frame` (MJPEG) |
| `GET /stream/terminal?camera=<path>` | Terminal output | `text/plain; charset=utf-8` (chunked) |


## License

MIT
