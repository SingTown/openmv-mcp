#include "mcp_server.h"

#include <iostream>

using json = nlohmann::json;

namespace mcp {

static json toolDefinitions() {
    return json::array();
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

    // Health check
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

    // No tools registered yet
    return makeError(id, -32602, "Unknown tool: " + name);
}

json McpServer::makeResponse(const json& id, const json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

json McpServer::makeError(const json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

}  // namespace mcp
