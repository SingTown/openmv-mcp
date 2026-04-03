#include "mcp_server.h"

#include <gtest/gtest.h>
#include <httplib/httplib.h>

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

    static json call_tool(int id, const std::string& name, const json& args = json::object()) {
        json req = {{"jsonrpc", "2.0"},
                    {"id", id},
                    {"method", "tools/call"},
                    {"params", {{"name", name}, {"arguments", args}}}};
        return post_mcp(req);
    }

    static constexpr int kPort = 18900;
    static std::unique_ptr<mcp::McpServer> server_;
    static std::thread server_thread_;
    static std::unique_ptr<httplib::Client> client_;
};

std::unique_ptr<mcp::McpServer> McpServerTest::server_;
std::thread McpServerTest::server_thread_;
std::unique_ptr<httplib::Client> McpServerTest::client_;

TEST_F(McpServerTest, Health) {
    auto res = client_->Get("/health");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    auto body = json::parse(res->body);
    EXPECT_EQ(body["status"], "ok");
}

TEST_F(McpServerTest, Initialize) {
    json req = {{"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "initialize"},
                {"params",
                 {{"protocolVersion", "2025-03-26"},
                  {"capabilities", json::object()},
                  {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}}}};
    auto resp = post_mcp(req);
    EXPECT_EQ(resp["jsonrpc"], "2.0");
    EXPECT_EQ(resp["id"], 1);
    ASSERT_TRUE(resp.contains("result"));
    auto& r = resp["result"];
    EXPECT_EQ(r["protocolVersion"], "2025-03-26");
    EXPECT_TRUE(r.contains("capabilities"));
    EXPECT_EQ(r["serverInfo"]["name"], "openmv-mcp-server");
}

TEST_F(McpServerTest, ToolsList) {
    json req = {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}};
    auto resp = post_mcp(req);
    ASSERT_TRUE(resp.contains("result"));
    auto& tools = resp["result"]["tools"];
    ASSERT_TRUE(tools.is_array());
    EXPECT_GE(tools.size(), 1u);
    bool found = false;
    for (const auto& t : tools) {
        if (t["name"] == "list_cameras") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(McpServerTest, Ping) {
    json req = {{"jsonrpc", "2.0"}, {"id", 3}, {"method", "ping"}};
    auto resp = post_mcp(req);
    EXPECT_EQ(resp["id"], 3);
    EXPECT_TRUE(resp.contains("result"));
}

TEST_F(McpServerTest, UnknownMethod) {
    json req = {{"jsonrpc", "2.0"}, {"id", 20}, {"method", "nonexistent"}};
    auto resp = post_mcp(req);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32601);
}

TEST_F(McpServerTest, InvalidJson) {
    auto res = client_->Post("/mcp", "not json{{{", "application/json");
    ASSERT_NE(res, nullptr);
    auto resp = json::parse(res->body);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32700);
}

TEST_F(McpServerTest, ListCameras) {
    auto resp = call_tool(40, "list_cameras");
    ASSERT_TRUE(resp.contains("result"));
    auto& content = resp["result"]["content"];
    ASSERT_TRUE(content.is_array());
    if (!content.empty()) {
        EXPECT_EQ(content[0]["type"], "text");
        auto cameras = json::parse(content[0]["text"].get<std::string>());
        EXPECT_TRUE(cameras.is_array());
    }
}

TEST_F(McpServerTest, UnknownTool) {
    auto resp = call_tool(30, "nonexistent_tool");
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32602);
}

TEST_F(McpServerTest, InvalidJsonRpcVersion) {
    json req = {{"jsonrpc", "1.0"}, {"id", 50}, {"method", "ping"}};
    auto resp = post_mcp(req);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32600);
}

TEST_F(McpServerTest, ConnectCameraInvalidPath) {
    auto resp = call_tool(60, "connect_camera", {{"path", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, DisconnectCameraNotConnected) {
    auto resp = call_tool(61, "disconnect_camera", {{"path", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, ConnectCameraMissingPath) {
    auto resp = call_tool(62, "connect_camera");
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, ToolsListIncludesNewTools) {
    json req = {{"jsonrpc", "2.0"}, {"id", 63}, {"method", "tools/list"}};
    auto resp = post_mcp(req);
    ASSERT_TRUE(resp.contains("result"));
    auto& tools = resp["result"]["tools"];
    ASSERT_TRUE(tools.is_array());
    EXPECT_GE(tools.size(), 3u);
    bool found_connect = false;
    bool found_disconnect = false;
    for (const auto& t : tools) {
        if (t["name"] == "connect_camera") found_connect = true;
        if (t["name"] == "disconnect_camera") found_disconnect = true;
    }
    EXPECT_TRUE(found_connect);
    EXPECT_TRUE(found_disconnect);
}
