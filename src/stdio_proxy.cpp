#include "stdio_proxy.h"

#include <httplib/httplib.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

namespace mcp {

using json = nlohmann::ordered_json;

namespace {

constexpr int kConnectionTimeoutMs = 5000;
constexpr int kReadTimeoutMs = 180000;

std::string makeProxyError(const std::string& message, int code, const std::string& text) {
    json id = nullptr;
    bool should_reply = false;

    auto request = json::parse(message, nullptr, false);
    if (request.is_discarded()) {
        should_reply = true;
    } else if (request.is_object() && request.contains("id")) {
        id = request["id"];
        should_reply = true;
    }

    if (!should_reply) {
        return {};
    }

    return json({{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", text}}}}).dump();
}

}  // namespace

int runStdioProxy(const std::string& host, int port) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        httplib::Client client(host, port);
        client.set_connection_timeout(kConnectionTimeoutMs / 1000, (kConnectionTimeoutMs % 1000) * 1000);
        client.set_read_timeout(kReadTimeoutMs / 1000, (kReadTimeoutMs % 1000) * 1000);

        std::string response;
        auto res = client.Post("/mcp", line, "application/json");
        if (!res) {
            response = makeProxyError(line, -32000, "Failed to reach openmv-mcp HTTP server");
        } else if (!res->body.empty()) {
            response = res->body;
        } else if (res->status != 202) {
            response = makeProxyError(line, -32000, "openmv-mcp HTTP server returned empty response");
        }

        if (!response.empty()) {
            std::cout << response << '\n' << std::flush;
        }
    }
    return 0;
}

}  // namespace mcp
