#include "server/mcp_tool.h"

#include "camera.h"
#include "camera_list/camera_list.h"

namespace mcp {

static const json CAMERA_PATH_PARAM = {
    {"cameraPath", {{"type", "string"}, {"description", "Serial port path of the camera"}}}};

static const json CAMERA_PATH_SCHEMA = {
    {"type", "object"}, {"properties", CAMERA_PATH_PARAM}, {"required", json::array({"cameraPath"})}};

static const McpTool TOOL_LIST_CAMERAS = {
    "list_cameras",
    "List connected OpenMV cameras",
    {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
    [](McpContext& /*ctx*/, const json& /*args*/) {
        auto cams = listCameras();
        json result = json::array();
        for (const auto& cam : cams) {
            result.push_back({{"path", cam.path}, {"displayName", cam.displayName}});
        }
        McpContent resp;
        resp.addText(result);
        return resp;
    },
};

static const McpTool TOOL_CAMERA_CONNECT = {
    "camera_connect",
    "Connect to an OpenMV camera by its serial port path",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args) {
        auto cameraPath = args.at("cameraPath").get<std::string>();
        auto camera = std::make_unique<Camera>();
        camera->connect(cameraPath);
        ctx.addCamera(cameraPath, std::move(camera));
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_CAMERA_DISCONNECT = {
    "camera_disconnect",
    "Disconnect from a connected camera",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args) {
        auto cameraPath = args.at("cameraPath").get<std::string>();
        ctx.removeCamera(cameraPath);
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_CAMERA_INFO = {
    "camera_info",
    "Get detailed information about a connected camera",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        McpContent resp;
        resp.addText(cam.systemInfo());
        return resp;
    },
};

static const McpTool TOOL_RUN_SCRIPT = {
    "run_script",
    "Execute a MicroPython script on the camera",
    {{"type", "object"},
     {"properties",
      {{"cameraPath", {{"type", "string"}, {"description", "Serial port path of the camera"}}},
       {"script",
        {{"type", "string"},
         {"description", "MicroPython source code to execute"},
         {"default",
          "import csi\nimport time\n\ncsi0 = csi.CSI()\ncsi0.reset()\n"
          "csi0.pixformat(csi.RGB565)\ncsi0.framesize(csi.VGA)\n"
          "csi0.snapshot(time=2000)\n\nclock = time.clock()\n\n"
          "while True:\n    clock.tick()\n    img = csi0.snapshot()\n"
          "    print(clock.fps())"}}}}},
     {"required", json::array({"cameraPath", "script"})}},
    [](McpContext& ctx, const json& args) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        cam.execScript(args.at("script").get<std::string>());
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_STOP_SCRIPT = {
    "stop_script",
    "Stop the currently running script on the camera",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        cam.stopScript();
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_READ_TERMINAL = {
    "read_terminal",
    "Read available terminal output (stdout/stderr) from the camera",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        McpContent resp;
        resp.addText(json({{"output", cam.readTerminal()}}));
        return resp;
    },
};

static const McpTool TOOL_SCRIPT_RUNNING = {
    "script_running",
    "Check if a script is currently running on the camera (updated by device events)",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        McpContent resp;
        resp.addText(json({{"running", cam.scriptRunning()}}));
        return resp;
    },
};

static const McpTool TOOL_READ_FRAME = {
    "read_frame",
    "Capture a single frame from the camera's frame buffer. A script that captures frames must be "
    "running. Returns the image as base64-encoded JPEG.",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        auto frame = cam.readFrame();
        if (!frame) {
            throw std::runtime_error("No frame available");
        }
        McpContent resp;
        resp.addImage(frame->toBase64Jpeg(), "image/jpeg");
        return resp;
    },
};

const std::map<std::string, const McpTool*> ALL_MCP_TOOLS = {
    {TOOL_LIST_CAMERAS.name, &TOOL_LIST_CAMERAS},
    {TOOL_CAMERA_CONNECT.name, &TOOL_CAMERA_CONNECT},
    {TOOL_CAMERA_DISCONNECT.name, &TOOL_CAMERA_DISCONNECT},
    {TOOL_CAMERA_INFO.name, &TOOL_CAMERA_INFO},
    {TOOL_RUN_SCRIPT.name, &TOOL_RUN_SCRIPT},
    {TOOL_STOP_SCRIPT.name, &TOOL_STOP_SCRIPT},
    {TOOL_READ_TERMINAL.name, &TOOL_READ_TERMINAL},
    {TOOL_SCRIPT_RUNNING.name, &TOOL_SCRIPT_RUNNING},
    {TOOL_READ_FRAME.name, &TOOL_READ_FRAME},
};

}  // namespace mcp
