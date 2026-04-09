#pragma once

#include "client/mcp_client.h"
#include "server/mcp_server.h"

#include <gtest/gtest.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

using json = nlohmann::json;

class McpServerTest : public ::testing::Test {
 protected:
    static void SetUpTestSuite() {
        server_ = std::make_unique<mcp::McpServer>(kPort);
        server_thread_ = std::thread([]() { server_->start(); });
        client_ = std::make_unique<mcp::McpClient>("127.0.0.1", kPort);
        for (int i = 0; i < 100; ++i) {
            if (client_->isHealthy()) {
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

    static constexpr int kPort = 18900;
    static inline std::unique_ptr<mcp::McpServer> server_;
    static inline std::thread server_thread_;
    static inline std::unique_ptr<mcp::McpClient> client_;
};
