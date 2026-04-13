#include "server/mcp_tool.h"

#include <atomic>
#include <fstream>
#include <thread>

#include "board.h"
#include "camera.h"
#include "camera_list/camera_list.h"
#include "firmware.h"

namespace mcp {

static const json CAMERA_PATH_PARAM = {
    {"cameraPath", {{"type", "string"}, {"description", "Serial port path of the camera"}}}};

static const json CAMERA_PATH_SCHEMA = {
    {"type", "object"}, {"properties", CAMERA_PATH_PARAM}, {"required", json::array({"cameraPath"})}};

static const McpTool TOOL_CAMERA_LIST = {
    "camera_list",
    "List OpenMV cameras with their connection state",
    {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
    [](McpContext& ctx, const json& /*args*/, const std::atomic<bool>& /*cancelled*/) {
        auto cams = listCameras();
        json result = json::array();
        for (const auto& cam : cams) {
            result.push_back({{"path", cam.path}, {"name", cam.name}, {"connected", ctx.hasCamera(cam.path)}});
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
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto cameraPath = args.at("cameraPath").get<std::string>();
        auto camera = Camera::create(cameraPath);
        auto& cam = ctx.addCamera(cameraPath, std::move(camera));
        std::thread([&cam]() { cam.info.checkLicense(); }).detach();
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_CAMERA_DISCONNECT = {
    "camera_disconnect",
    "Disconnect from a connected camera",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
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
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        const auto& si = cam.info;

        McpContent resp;
        resp.addText(json({{"cameraPath", si.cameraPath()},
                           {"drivePath", si.drivePath().string()},
                           {"boardId", si.deviceIdHex()},
                           {"boardType", si.boardType()},
                           {"name", si.boardName()},
                           {"sensor", si.sensorString()},
                           {"fwVersion", si.fwVersionString()},
                           {"latestFwVersion", latestFirmwareVersion()},
                           {"protocolVersion", si.protocolVersion()}}));
        return resp;
    },
};

static const McpTool TOOL_SCRIPT_RUN = {
    "script_run",
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
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        if (!cam.info.licensed()) {
            throw std::runtime_error(
                "Script execution failed: Unregistered OpenMV Cam detected. "
                "You need to register your OpenMV Cam with OpenMV for unlimited use. "
                "If you do not have a board key you may purchase one from OpenMV "
                "(https://openmv.io/products/openmv-cam-board-key). "
                "Please use the license_register tool with your board key to register.");
        }
        cam.execScript(args.at("script").get<std::string>());
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_SCRIPT_STOP = {
    "script_stop",
    "Stop the currently running script on the camera",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        cam.stopScript();
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_SCRIPT_OUTPUT = {
    "script_output",
    "Read available script output (stdout/stderr) from the camera",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
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
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        McpContent resp;
        resp.addText(json({{"running", cam.scriptRunning()}}));
        return resp;
    },
};

static const McpTool TOOL_SCRIPT_SAVE = {
    "script_save",
    "Save a MicroPython script to main.py on the camera's USB drive so it runs on boot",
    {{"type", "object"},
     {"properties",
      {{"cameraPath", {{"type", "string"}, {"description", "Serial port path of the camera"}}},
       {"script", {{"type", "string"}, {"description", "MicroPython source code to save as main.py"}}}}},
     {"required", json::array({"cameraPath", "script"})}},
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        auto drive = cam.info.drivePath();
        if (drive.empty()) {
            throw std::runtime_error(
                "Script save failed: Camera USB drive not found. "
                "Make sure the camera is connected and the USB drive is mounted.");
        }
        auto filepath = drive / "main.py";
        std::ofstream ofs(filepath, std::ios::trunc);
        if (!ofs) {
            throw std::runtime_error("Script save failed: Cannot open " + filepath.string() + " for writing.");
        }
        ofs << args.at("script").get<std::string>();
        ofs.close();
        if (!ofs) {
            throw std::runtime_error("Script save failed: Error writing to " + filepath.string());
        }
        McpContent resp;
        resp.addText(json({{"success", true}, {"path", filepath.string()}}));
        return resp;
    },
};

static const McpTool TOOL_CAMERA_RESET = {
    "camera_reset",
    "Reset (reboot) the camera",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto cameraPath = args.at("cameraPath").get<std::string>();
        auto camera = ctx.takeCamera(cameraPath);
        camera->reset();
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_FRAME_CAPTURE = {
    "frame_capture",
    "Capture a single frame from the camera's frame buffer. A script that captures frames must be "
    "running. Returns the image as base64-encoded JPEG.",
    CAMERA_PATH_SCHEMA,
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
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

static json boardEnum() {
    json result = json::array();
    for (const auto& b : allBoards()) {
        if (!b.firmwareCommands.empty()) {
            result.push_back(b.name);
        }
    }
    return result;
}

static const McpTool TOOL_FIRMWARE_FLASH = {
    "firmware_flash",
    "Flash firmware to the camera. If firmwareDir is omitted or empty, upgrades to the default (latest) firmware.",
    {{"type", "object"},
     {"properties",
      {{"cameraPath", {{"type", "string"}, {"description", "Serial port path of the camera"}}},
       {"firmwareDir",
        {{"type", "string"},
         {"description", "Path to the firmware directory. If omitted, uses the default firmware."}}}}},
     {"required", json::array({"cameraPath"})}},
    [](McpContext& ctx, const json& args, const std::atomic<bool>& cancelled) {
        auto cameraPath = args.at("cameraPath").get<std::string>();
        auto camera = ctx.takeCamera(cameraPath);
        auto name = camera->info.boardName();
        if (name.empty()) {
            throw std::runtime_error("Camera board name is unknown; cannot determine firmware target");
        }
        auto firmwareDir = args.value("firmwareDir", std::string{});
        camera->boot();
        camera.reset();
        auto onNotice = [&](const std::string& msg) { ctx.streamMessage(msg, "notice"); };
        auto onDebug = [&](const std::string& msg) { ctx.streamMessage(msg, "debug"); };
        firmwareFlash(name, firmwareDir, onNotice, onDebug, cancelled);
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
    true,
};

static const McpTool TOOL_FIRMWARE_REPAIR = {
    "firmware_repair",
    "Fully repair an OpenMV camera by flashing both bootloader and firmware",
    {{"type", "object"},
     {"properties", {{"name", {{"type", "string"}, {"description", "Board name"}, {"enum", boardEnum()}}}}},
     {"required", json::array({"name"})}},
    [](McpContext& ctx, const json& args, const std::atomic<bool>& cancelled) {
        auto name = args.at("name").get<std::string>();
        auto onNotice = [&](const std::string& msg) { ctx.streamMessage(msg, "notice"); };
        auto onDebug = [&](const std::string& msg) { ctx.streamMessage(msg, "debug"); };
        firmwareRepair(name, onNotice, onDebug, cancelled);
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
    true,
};

static const McpTool TOOL_LICENSE_REGISTER = {
    "license_register",
    "Register a board key for the connected camera",
    {{"type", "object"},
     {"properties",
      {{"cameraPath", {{"type", "string"}, {"description", "Serial port path of the camera"}}},
       {"boardKey",
        {{"type", "string"},
         {"description", "Board key for registration (format: XXXXX-XXXXX-XXXXX-XXXXX-XXXXX)"},
         {"pattern", "^[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}$"}}}}},
     {"required", json::array({"cameraPath", "boardKey"})}},
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        cam.info.registerLicense(args.at("boardKey").get<std::string>());
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

static const McpTool TOOL_FRAME_ENABLE = {
    "frame_enable",
    "Enable or disable frame streaming from the camera",
    {{"type", "object"},
     {"properties",
      {{"cameraPath", {{"type", "string"}, {"description", "Serial port path of the camera"}}},
       {"enable", {{"type", "boolean"}, {"description", "true to enable, false to disable"}}}}},
     {"required", json::array({"cameraPath", "enable"})}},
    [](McpContext& ctx, const json& args, const std::atomic<bool>& /*cancelled*/) {
        auto& cam = ctx.getCamera(args.at("cameraPath").get<std::string>());
        cam.enableFrame(args.at("enable").get<bool>());
        McpContent resp;
        resp.addText(json({{"success", true}}));
        return resp;
    },
};

const std::vector<const McpTool*> ALL_MCP_TOOLS = {
    &TOOL_CAMERA_LIST,
    &TOOL_CAMERA_CONNECT,
    &TOOL_CAMERA_DISCONNECT,
    &TOOL_CAMERA_RESET,
    &TOOL_CAMERA_INFO,
    &TOOL_SCRIPT_RUN,
    &TOOL_SCRIPT_STOP,
    &TOOL_SCRIPT_OUTPUT,
    &TOOL_SCRIPT_RUNNING,
    &TOOL_SCRIPT_SAVE,
    &TOOL_FRAME_CAPTURE,
    &TOOL_FRAME_ENABLE,
    &TOOL_FIRMWARE_FLASH,
    &TOOL_FIRMWARE_REPAIR,
    &TOOL_LICENSE_REGISTER,
};

}  // namespace mcp
