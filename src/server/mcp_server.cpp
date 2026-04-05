#include "server/mcp_server.h"

#include <iostream>

namespace mcp {

McpServer::McpServer(int port) : port_(port), ctx_(std::make_unique<McpContext>()) {}

McpServer::~McpServer() {
    server_.stop();
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
    ctx_.reset();
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
        return handleInitialize(id);
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

json McpServer::handleInitialize(const json& id) {
    return makeResponse(id,
                        {{"protocolVersion", "2025-03-26"},
                         {"capabilities", {{"tools", json::object()}}},
                         {"serverInfo", {{"name", "openmv-mcp-server"}, {"version", "1.0.0"}}}});
}

json McpServer::handleToolsList(const json& id) {
    static const json tools = [] {
        json arr = json::array();
        for (const auto& [name, tool] : ALL_MCP_TOOLS) {
            arr.push_back(
                {{"name", tool->name}, {"description", tool->description}, {"inputSchema", tool->input_schema}});
        }
        return arr;
    }();
    return makeResponse(id, {{"tools", tools}});
}

json McpServer::handleToolsCall(const json& params, const json& id) {
    auto name = params.value("name", "");
    auto args = params.value("arguments", json::object());

    auto it = ALL_MCP_TOOLS.find(name);
    if (it == ALL_MCP_TOOLS.end()) {
        return makeError(id, -32602, "Unknown tool: " + name);
    }

    try {
        auto resp = it->second->handler(*ctx_, args);
        return makeResponse(id, {{"content", resp.toContent()}});
    } catch (const std::exception& e) {
        McpContent err;
        err.addText(json({{"error", std::string(e.what())}}));
        return makeResponse(id, {{"content", err.toContent()}, {"isError", true}});
    }
}

json McpServer::makeResponse(const json& id, const json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

json McpServer::makeError(const json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

}  // namespace mcp
