#pragma once

#include "server/mcp_server.h"

#include <gtest/gtest.h>
#include <httplib/httplib.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <thread>

using json = nlohmann::json;

class McpServerTest : public ::testing::Test {
 protected:
    static void SetUpTestSuite() {
        server_ = std::make_unique<mcp::McpServer>(kPort);
        server_thread_ = std::thread([]() { server_->start(); });
        client_ = std::make_unique<httplib::Client>("127.0.0.1", kPort);
        client_->set_connection_timeout(5);
        client_->set_read_timeout(10);
        for (int i = 0; i < 100; ++i) {
            if (client_->Get("/health")) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        FAIL() << "Server did not start within 2 seconds";
    }

    static void TearDownTestSuite() {
        client_.reset();
        server_->stop();
        server_thread_.join();
        server_.reset();
    }

    static json post_mcp(const json& request) {
        auto res = client_->Post("/mcp", request.dump(), "application/json");
        if (!res || res->body.empty()) return nullptr;
        return json::parse(res->body);
    }

    static json call_tool(const std::string& name, const json& args = json::object()) {
        json req = {{"jsonrpc", "2.0"},
                    {"id", next_id_++},
                    {"method", "tools/call"},
                    {"params", {{"name", name}, {"arguments", args}}}};
        return post_mcp(req);
    }

    static constexpr int kPort = 18900;
    static inline int next_id_ = 1;
    static inline std::unique_ptr<mcp::McpServer> server_;
    static inline std::thread server_thread_;
    static inline std::unique_ptr<httplib::Client> client_;
};
