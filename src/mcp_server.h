#pragma once
#include <httplib/httplib.h>

#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace mcp {

class Camera;

class McpServer {
 public:
    explicit McpServer(int port = 15257);
    ~McpServer();

    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    void start();
    void stop();

 private:
    nlohmann::json handleRequest(const nlohmann::json& request);
    nlohmann::json handleInitialize(const nlohmann::json& params, const nlohmann::json& id);
    nlohmann::json handleToolsList(const nlohmann::json& id);
    nlohmann::json handleToolsCall(const nlohmann::json& params, const nlohmann::json& id);

    static nlohmann::json makeResponse(const nlohmann::json& id, const nlohmann::json& result);
    static nlohmann::json makeError(const nlohmann::json& id, int code, const std::string& message);
    static nlohmann::json makeToolError(const nlohmann::json& id, const std::string& message);

    int port_;
    httplib::Server server_;
    std::map<std::string, std::unique_ptr<Camera>> cameras_;
    std::mutex cameras_mutex_;
};

}  // namespace mcp
