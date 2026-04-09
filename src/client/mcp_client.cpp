#include "client/mcp_client.h"

#include <sstream>
#include <utility>

namespace mcp {

// --- ToolResponse ---

ToolResponse::ToolResponse(ToolResponse&& other) noexcept
    : request_id_(other.request_id_), state_(std::move(other.state_)), thread_(std::move(other.thread_)) {
    other.request_id_ = 0;
}

ToolResponse& ToolResponse::operator=(ToolResponse&& other) noexcept {
    if (this != &other) {
        join();
        request_id_ = other.request_id_;
        state_ = std::move(other.state_);
        thread_ = std::move(other.thread_);
        other.request_id_ = 0;
    }
    return *this;
}

ToolResponse::~ToolResponse() {
    join();
}

void ToolResponse::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

ToolResult ToolResponse::wait() {
    join();
    if (state_->error) {
        std::rethrow_exception(state_->error);
    }
    ToolResult result;
    result.is_error = state_->result.value("isError", false);
    if (state_->result.contains("content")) {
        for (const auto& item : state_->result["content"]) {
            ContentItem ci;
            ci.type = item.value("type", "");
            ci.text = item.value("text", "");
            ci.data = item.value("data", "");
            ci.mime_type = item.value("mimeType", "");
            result.content.push_back(std::move(ci));
        }
    }
    return result;
}

const std::vector<json>& ToolResponse::notifications() {
    join();
    return state_->notifications;
}

// --- McpClient ---

McpClient::McpClient(const std::string& host, int port) : host_(host), port_(port), client_(host, port) {
    client_.set_connection_timeout(5);
    client_.set_read_timeout(30);
}

void McpClient::initialize() {
    sendRequest("initialize",
                {{"protocolVersion", "2025-03-26"},
                 {"capabilities", json::object()},
                 {"clientInfo", {{"name", "mcp-test-client"}, {"version", "1.0.0"}}}});
    sendNotification("notifications/initialized");
}

std::vector<ToolInfo> McpClient::listTools() {
    auto result = sendRequest("tools/list");
    std::vector<ToolInfo> tools;
    for (const auto& t : result["tools"]) {
        tools.push_back(
            {t["name"].get<std::string>(), t.value("description", ""), t.value("inputSchema", json::object())});
    }
    return tools;
}

ToolResponse McpClient::callTool(const std::string& name, const json& args) {
    ToolResponse resp;
    resp.request_id_ = next_id_;
    resp.state_ = std::make_shared<ToolResponse::State>();
    auto request = buildRequest("tools/call", {{"name", name}, {"arguments", args}});

    resp.thread_ = std::thread(executeToolCall, host_, port_, request.dump(), resp.state_);
    return resp;
}

void McpClient::executeToolCall(const std::string& host,
                                int port,
                                const std::string& request_body,
                                const std::shared_ptr<ToolResponse::State>& state) {
    try {
        httplib::Client client(host, port);
        client.set_connection_timeout(5);
        client.set_read_timeout(180);

        std::string body;
        auto res = client.Post(
            "/mcp", httplib::Headers{}, request_body, "application/json", [&body](const char* data, size_t len) {
                body.append(data, len);
                return true;
            });

        if (!res) {
            throw std::runtime_error("HTTP request failed");
        }

        auto content_type = res->get_header_value("Content-Type");

        if (content_type.find("text/event-stream") != std::string::npos) {
            auto events = parseSseEvents(body);
            for (auto& event : events) {
                if (event.contains("result")) {
                    state->result = std::move(event["result"]);
                } else {
                    state->notifications.push_back(std::move(event));
                }
            }
            if (state->result.is_null()) {
                throw std::runtime_error("No result in SSE stream");
            }
            return;
        }

        auto response = json::parse(body);
        state->result = extractResult(response);
    } catch (...) {
        state->error = std::current_exception();
    }
}

void McpClient::cancelTool(int requestId) {
    sendNotification("notifications/cancelled", {{"requestId", requestId}});
}

void McpClient::ping() {
    sendRequest("ping");
}

json McpClient::sendRequest(const std::string& method, const json& params) {
    auto request = buildRequest(method, params);
    auto res = client_.Post("/mcp", request.dump(), "application/json");
    if (!res || res->body.empty()) {
        throw std::runtime_error("HTTP request failed");
    }
    auto response = json::parse(res->body);
    return extractResult(response);
}

void McpClient::sendNotification(const std::string& method, const json& params) {
    auto notification = buildNotification(method, params);
    client_.Post("/mcp", notification.dump(), "application/json");
}

bool McpClient::isHealthy() {
    auto res = client_.Get("/health");
    return res && res->status == 200;
}

json McpClient::buildRequest(const std::string& method, const json& params) {
    return {{"jsonrpc", "2.0"}, {"id", next_id_++}, {"method", method}, {"params", params}};
}

json McpClient::buildNotification(const std::string& method, const json& params) {
    return {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
}

json McpClient::extractResult(const json& response) {
    if (response.contains("error")) {
        const auto& err = response["error"];
        throw std::runtime_error(err.value("message", std::string("Unknown error")));
    }
    return response.value("result", json(nullptr));
}

std::vector<json> McpClient::parseSseEvents(const std::string& body) {
    std::vector<json> events;
    std::istringstream stream(body);
    std::string line;
    std::string data_line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("data: ", 0) == 0) {
            data_line = line.substr(6);
        } else if (line.empty() && !data_line.empty()) {
            events.push_back(json::parse(data_line));
            data_line.clear();
        }
    }
    if (!data_line.empty()) {
        events.push_back(json::parse(data_line));
    }
    return events;
}

}  // namespace mcp
