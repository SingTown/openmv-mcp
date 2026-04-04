#include "mcp_server.h"

#include <iostream>

#include "camera.h"
#include "camera_list/camera_list.h"

using json = nlohmann::json;

namespace mcp {

static json cameraPathParam() {
    return {{"cameraPath", {{"type", "string"}, {"description", "Serial port path of the camera"}}}};
}

static const json& toolDefinitions() {
    static const json defs = json::array({
        {{"name", "list_cameras"},
         {"description", "List connected OpenMV cameras"},
         {"inputSchema", {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}}}},

        {{"name", "camera_connect"},
         {"description", "Connect to an OpenMV camera by its serial port path"},
         {"inputSchema",
          {{"type", "object"}, {"properties", cameraPathParam()}, {"required", json::array({"cameraPath"})}}}},

        {{"name", "camera_disconnect"},
         {"description", "Disconnect from a connected camera"},
         {"inputSchema",
          {{"type", "object"}, {"properties", cameraPathParam()}, {"required", json::array({"cameraPath"})}}}},

        {{"name", "camera_info"},
         {"description", "Get detailed information about a connected camera"},
         {"inputSchema",
          {{"type", "object"}, {"properties", cameraPathParam()}, {"required", json::array({"cameraPath"})}}}},

        {{"name", "run_script"},
         {"description", "Execute a MicroPython script on the camera"},
         {"inputSchema",
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
           {"required", json::array({"cameraPath", "script"})}}}},

        {{"name", "stop_script"},
         {"description", "Stop the currently running script on the camera"},
         {"inputSchema",
          {{"type", "object"}, {"properties", cameraPathParam()}, {"required", json::array({"cameraPath"})}}}},

        {{"name", "read_terminal"},
         {"description", "Read available terminal output (stdout/stderr) from the camera"},
         {"inputSchema",
          {{"type", "object"}, {"properties", cameraPathParam()}, {"required", json::array({"cameraPath"})}}}},

        {{"name", "script_running"},
         {"description", "Check if a script is currently running on the camera (updated by device events)"},
         {"inputSchema",
          {{"type", "object"}, {"properties", cameraPathParam()}, {"required", json::array({"cameraPath"})}}}},
    });
    return defs;
}

McpServer::McpServer(int port) : port_(port) {}

McpServer::~McpServer() {
    shutdown();
}

void McpServer::start() {
    server_.new_task_queue = [] { return new httplib::ThreadPool(1); };

    server_.Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");
        try {
            auto request = json::parse(req.body);
            auto response = handleRequest(request);
            if (response.is_null()) {
                res.status = 202;
                return;
            }
            res.set_content(response.dump(), "application/json");
        } catch (const json::parse_error& e) {
            auto err = makeError(nullptr, -32700, std::string("Parse error: ") + e.what());
            res.set_content(err.dump(), "application/json");
        }
    });

    server_.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    std::cout << "MCP Server listening on port " << port_ << '\n';
    server_.listen("127.0.0.1", port_);
}

void McpServer::stopListening() {
    server_.stop();
}

void McpServer::shutdown() {
    if (stopped_.exchange(true)) return;

    std::cerr << "Shutting down...\n";

    // Stop accepting new connections and wait for the ThreadPool(1) worker to
    // finish any in-flight request.  After this call returns the worker thread
    // has been joined, so no handler can be accessing cameras_ concurrently.
    server_.stop();

    for (auto& [path, camera] : cameras_) {
        try {
            camera->disconnect();
        } catch (...) {
        }
    }
    cameras_.clear();
}

json McpServer::handleRequest(const json& request) {
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return makeError(request.value("id", json(nullptr)), -32600, "Invalid JSON-RPC version");
    }

    auto method = request.value("method", "");
    auto id = request.value("id", json(nullptr));
    auto params = request.value("params", json::object());

    if (method == "initialize") {
        return handleInitialize(params, id);
    }
    if (method == "notifications/initialized") {
        return json(nullptr);
    }
    if (method == "tools/list") {
        return handleToolsList(id);
    }
    if (method == "tools/call") {
        return handleToolsCall(params, id);
    }
    if (method == "ping") {
        return makeResponse(id, json::object());
    }

    return makeError(id, -32601, "Method not found: " + method);
}

json McpServer::handleInitialize(const json& /*params*/, const json& id) {
    return makeResponse(id,
                        {{"protocolVersion", "2025-03-26"},
                         {"capabilities", {{"tools", json::object()}}},
                         {"serverInfo", {{"name", "openmv-mcp-server"}, {"version", "1.0.0"}}}});
}

json McpServer::handleToolsList(const json& id) {
    return makeResponse(id, {{"tools", toolDefinitions()}});
}

json McpServer::handleToolsCall(const json& params, const json& id) {
    auto name = params.value("name", "");
    auto args = params.value("arguments", json::object());

    try {
        json result;
        if (name == "list_cameras") {
            result = toolListCameras(args);
        } else if (name == "camera_connect") {
            result = toolCameraConnect(args);
        } else if (name == "camera_disconnect") {
            result = toolCameraDisconnect(args);
        } else if (name == "camera_info") {
            result = toolCameraInfo(args);
        } else if (name == "run_script") {
            result = toolRunScript(args);
        } else if (name == "stop_script") {
            result = toolStopScript(args);
        } else if (name == "read_terminal") {
            result = toolReadTerminal(args);
        } else if (name == "script_running") {
            result = toolScriptRunning(args);
        } else {
            return makeError(id, -32602, "Unknown tool: " + name);
        }

        return makeResponse(id, {{"content", json::array({{{"type", "text"}, {"text", result.dump()}}})}});
    } catch (const std::exception& e) {
        return makeResponse(
            id,
            {{"content", json::array({{{"type", "text"}, {"text", std::string("Error: ") + e.what()}}})},
             {"isError", true}});
    }
}

json McpServer::toolListCameras(const json& /*args*/) {
    auto cameras = listCameras();
    json result = json::array();
    for (const auto& cam : cameras) {
        result.push_back({{"path", cam.path}, {"displayName", cam.displayName}});
    }
    return result;
}

json McpServer::toolCameraConnect(const json& args) {
    auto cameraPath = args.at("cameraPath").get<std::string>();

    if (cameras_.count(cameraPath)) {
        throw std::runtime_error("Camera already connected: " + cameraPath);
    }

    auto camera = std::make_unique<Camera>();
    camera->connect(cameraPath);

    cameras_[cameraPath] = std::move(camera);
    return {{"success", true}};
}

json McpServer::toolCameraDisconnect(const json& args) {
    auto cameraPath = args.at("cameraPath").get<std::string>();

    auto it = cameras_.find(cameraPath);
    if (it == cameras_.end()) {
        throw std::runtime_error("Camera not connected: " + cameraPath);
    }
    it->second->disconnect();
    cameras_.erase(it);
    return {{"success", true}};
}

json McpServer::toolCameraInfo(const json& args) {
    auto cameraPath = args.at("cameraPath").get<std::string>();
    auto& cam = getCamera(cameraPath);
    return cam.systemInfo();
}

json McpServer::toolRunScript(const json& args) {
    auto cameraPath = args.at("cameraPath").get<std::string>();
    auto script = args.at("script").get<std::string>();
    auto& cam = getCamera(cameraPath);
    cam.execScript(script);
    return {{"success", true}};
}

json McpServer::toolStopScript(const json& args) {
    auto cameraPath = args.at("cameraPath").get<std::string>();
    auto& cam = getCamera(cameraPath);
    cam.stopScript();
    return {{"success", true}};
}

json McpServer::toolReadTerminal(const json& args) {
    auto cameraPath = args.at("cameraPath").get<std::string>();
    auto& cam = getCamera(cameraPath);
    std::string output = cam.readTerminal();
    return {{"output", output}};
}

json McpServer::toolScriptRunning(const json& args) {
    auto cameraPath = args.at("cameraPath").get<std::string>();
    auto& cam = getCamera(cameraPath);
    return {{"running", cam.scriptRunning()}};
}

Camera& McpServer::getCamera(const std::string& cameraPath) {
    auto it = cameras_.find(cameraPath);
    if (it == cameras_.end()) {
        throw std::runtime_error("Camera not connected: " + cameraPath);
    }
    return *it->second;
}

json McpServer::makeResponse(const json& id, const json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

json McpServer::makeError(const json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

}  // namespace mcp
