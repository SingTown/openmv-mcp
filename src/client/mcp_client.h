#pragma once

#include <httplib/httplib.h>

#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace mcp {

using json = nlohmann::json;

struct ContentItem {
    std::string type;
    std::string text;
    std::string data;
    std::string mime_type;
};

struct ToolResult {
    bool is_error = false;
    std::vector<ContentItem> content;
};

class ToolResponse {
 public:
    ToolResponse() = default;
    ToolResponse(ToolResponse&& other) noexcept;
    ToolResponse& operator=(ToolResponse&& other) noexcept;
    ~ToolResponse();

    ToolResponse(const ToolResponse&) = delete;
    ToolResponse& operator=(const ToolResponse&) = delete;

    ToolResult wait();
    const std::vector<json>& notifications();
    [[nodiscard]] int requestId() const { return request_id_; }

 private:
    friend class McpClient;

    struct State {
        json result;
        std::vector<json> notifications;
        std::exception_ptr error;
    };

    int request_id_ = 0;
    std::shared_ptr<State> state_;
    std::thread thread_;

    void join();
};

struct ToolInfo {
    std::string name;
    std::string description;
    json input_schema;
};

class McpClient {
 public:
    explicit McpClient(const std::string& host, int port);

    void initialize();
    std::vector<ToolInfo> listTools();
    ToolResponse callTool(const std::string& name, const json& args = json::object());
    void ping();

    bool isHealthy();

 private:
    json sendRequest(const std::string& method, const json& params = json::object());
    void sendNotification(const std::string& method, const json& params = json::object());
    json buildRequest(const std::string& method, const json& params);
    static json buildNotification(const std::string& method, const json& params);
    static json extractResult(const json& response);
    static std::vector<json> parseSseEvents(const std::string& body);

    static void executeToolCall(const std::string& host,
                                int port,
                                const std::string& request_body,
                                const std::shared_ptr<ToolResponse::State>& state);

    std::string host_;
    int port_;
    httplib::Client client_;
    std::atomic<int> next_id_{1};
};

}  // namespace mcp
