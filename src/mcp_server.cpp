#include "mcp_server.h"

#include <iostream>

#include "camera.h"
#include "camera_list/camera_list.h"

using json = nlohmann::json;

namespace mcp {

static const json& toolDefinitions() {
    static const json defs = json::array({
        {{"name", "list_cameras"},
         {"description", "Discover connected OpenMV cameras via USB serial port enumeration"},
         {"inputSchema", {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}}}},
        {{"name", "connect_camera"},
         {"description", "Connect to an OpenMV camera by opening its serial port"},
         {"inputSchema",
          {{"type", "object"},
           {"properties",
            {{"path", {{"type", "string"}, {"description", "Serial port path (e.g. /dev/cu.usbmodem1234)"}}}}},
           {"required", json::array({"path"})}}}},
        {{"name", "disconnect_camera"},
         {"description", "Disconnect from a connected OpenMV camera"},
         {"inputSchema",
          {{"type", "object"},
           {"properties",
            {{"path", {{"type", "string"}, {"description", "Serial port path of the connected camera"}}}}},
           {"required", json::array({"path"})}}}},
    });
    return defs;
}

McpServer::McpServer(int port) : port_(port) {}

McpServer::~McpServer() {
    stop();
}

void McpServer::start() {
    server_.Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
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

void McpServer::stop() {
    server_.stop();
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

    if (name == "list_cameras") {
        auto cameras = listCameras();
        json arr = json::array();
        for (const auto& cam : cameras) {
            arr.push_back({{"path", cam.path}, {"displayName", cam.displayName}});
        }
        json content = json::array({{{"type", "text"}, {"text", arr.dump(2)}}});
        return makeResponse(id, {{"content", content}});
    }

    if (name == "connect_camera") {
        auto args = params.value("arguments", json::object());
        auto path = args.value("path", "");
        if (path.empty()) {
            return makeToolError(id, "Missing required parameter: path");
        }

        std::lock_guard<std::mutex> lock(cameras_mutex_);
        auto it = cameras_.find(path);
        if (it != cameras_.end()) {
            return makeToolError(id, "Camera already connected: " + path);
        }

        auto camera = std::make_unique<Camera>();
        if (!camera->connect(path)) {
            return makeToolError(id, "Failed to connect to " + path);
        }

        cameras_.emplace(path, std::move(camera));
        json content = json::array({{{"type", "text"}, {"text", "Connected to " + path}}});
        return makeResponse(id, {{"content", content}});
    }

    if (name == "disconnect_camera") {
        auto args = params.value("arguments", json::object());
        auto path = args.value("path", "");
        if (path.empty()) {
            return makeToolError(id, "Missing required parameter: path");
        }

        std::lock_guard<std::mutex> lock(cameras_mutex_);
        auto it = cameras_.find(path);
        if (it == cameras_.end()) {
            return makeToolError(id, "Camera not connected: " + path);
        }

        cameras_.erase(it);
        json content = json::array({{{"type", "text"}, {"text", "Disconnected from " + path}}});
        return makeResponse(id, {{"content", content}});
    }

    return makeError(id, -32602, "Unknown tool: " + name);
}

json McpServer::makeResponse(const json& id, const json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

json McpServer::makeError(const json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

json McpServer::makeToolError(const json& id, const std::string& message) {
    json content = json::array({{{"type", "text"}, {"text", message}}});
    return makeResponse(id, {{"content", content}, {"isError", true}});
}

}  // namespace mcp
