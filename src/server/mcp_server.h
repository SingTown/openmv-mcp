#pragma once

#include <httplib/httplib.h>

#include <memory>
#include <nlohmann/json.hpp>

#include "server/mcp_tool.h"

namespace mcp {

class McpServer {
 public:
    explicit McpServer(int port = 15257);
    ~McpServer();

    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    bool bind();
    void start();
    void stop();

 private:
    void setupRoutes();
    void setupWebSocket();
    json handleRequest(const json& request);
    static json handleInitialize(const json& id);
    json handleToolsList(const json& id);
    json handleToolsCall(const json& params, const json& id);
    static const McpTool* findTool(const std::string& name);
    static json makeResponse(const json& id, const json& result);
    static json makeError(const json& id, int code, const std::string& message);

    int port_;
    httplib::Server server_;
    std::unique_ptr<McpContext> ctx_;
};

}  // namespace mcp
