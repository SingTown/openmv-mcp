# OpenMV MCP Server

An [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server for controlling OpenMV cameras over HTTP + JSON-RPC 2.0.

## Run

```bash
./build/openmv_mcp_server              # default port 15257
./build/openmv_mcp_server --port 9000  # custom port
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


## License

MIT
