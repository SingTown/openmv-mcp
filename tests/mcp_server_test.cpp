#include "mcp_server_test.h"

#include <httplib/httplib.h>

#include <set>

// =======================================================================
// Protocol tests
// =======================================================================

TEST_F(McpServerTest, Initialize) {
    EXPECT_NO_THROW(client_->initialize());
}

TEST_F(McpServerTest, Ping) {
    EXPECT_NO_THROW(client_->ping());
}

TEST_F(McpServerTest, InvalidJson) {
    httplib::Client http("127.0.0.1", kPort);
    auto res = http.Post("/mcp", "not json{{{", "application/json");
    ASSERT_NE(res, nullptr);
    auto resp = json::parse(res->body);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32700);
}

TEST_F(McpServerTest, InvalidJsonRpcVersion) {
    httplib::Client http("127.0.0.1", kPort);
    json req = {{"jsonrpc", "1.0"}, {"id", 9999}, {"method", "ping"}};
    auto res = http.Post("/mcp", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    auto resp = json::parse(res->body);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32600);
}

TEST(McpServerShutdown, PostShutdownStopsServer) {
    constexpr int kShutdownPort = 18901;
    mcp::McpServer server(kShutdownPort);
    ASSERT_TRUE(server.bind());
    std::thread th([&] { server.start(); });

    mcp::McpClient probe("127.0.0.1", kShutdownPort);
    bool ready = false;
    for (int i = 0; i < 100; ++i) {
        try {
            probe.ping();
            ready = true;
            break;
        } catch (const std::exception&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    ASSERT_TRUE(ready);

    httplib::Client http("127.0.0.1", kShutdownPort);
    auto res = http.Post("/shutdown", "", "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    auto body = json::parse(res->body);
    EXPECT_EQ(body.value("status", ""), "stopping");

    th.join();

    mcp::McpClient probe2("127.0.0.1", kShutdownPort);
    EXPECT_THROW(probe2.ping(), std::runtime_error);
}

TEST_F(McpServerTest, UnknownMethod) {
    httplib::Client http("127.0.0.1", kPort);
    json req = {{"jsonrpc", "2.0"}, {"id", 9999}, {"method", "nonexistent"}};
    auto res = http.Post("/mcp", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    auto resp = json::parse(res->body);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32601);
}

// =======================================================================
// Tool discovery & basic tool tests
// =======================================================================

TEST_F(McpServerTest, ToolsList) {
    auto tools = client_->listTools();
    EXPECT_EQ(tools.size(), 15U);

    std::set<std::string> names;
    for (const auto& t : tools) names.insert(t.name);
    EXPECT_TRUE(names.count("camera_list"));
    EXPECT_TRUE(names.count("camera_connect"));
    EXPECT_TRUE(names.count("camera_disconnect"));
    EXPECT_TRUE(names.count("camera_info"));
    EXPECT_TRUE(names.count("camera_reset"));
    EXPECT_TRUE(names.count("script_run"));
    EXPECT_TRUE(names.count("script_stop"));
    EXPECT_TRUE(names.count("script_output"));
    EXPECT_TRUE(names.count("script_running"));
    EXPECT_TRUE(names.count("frame_capture"));
    EXPECT_TRUE(names.count("frame_enable"));
    EXPECT_TRUE(names.count("script_save"));
    EXPECT_TRUE(names.count("firmware_flash"));
    EXPECT_TRUE(names.count("firmware_repair"));
}

TEST_F(McpServerTest, UnknownTool) {
    EXPECT_THROW(client_->callTool("nonexistent_tool").wait(), std::runtime_error);
}

TEST_F(McpServerTest, ListCameras) {
    auto result = client_->callTool("camera_list").wait();
    ASSERT_FALSE(result.content.empty());
    EXPECT_EQ(result.content[0].type, "text");
    auto cameras = json::parse(result.content[0].text);
    EXPECT_TRUE(cameras.is_array());
}

// =======================================================================
// Not-connected error tests
// =======================================================================

TEST_F(McpServerTest, CameraConnectInvalidPath) {
    auto result = client_->callTool("camera_connect", {{"cameraPath", "/dev/cu.nonexistent"}}).wait();
    EXPECT_TRUE(result.is_error);
}

TEST_F(McpServerTest, CameraDisconnectNotConnected) {
    auto result = client_->callTool("camera_disconnect", {{"cameraPath", "/dev/cu.nonexistent"}}).wait();
    EXPECT_TRUE(result.is_error);
}

TEST_F(McpServerTest, CameraInfoNotConnected) {
    auto result = client_->callTool("camera_info", {{"cameraPath", "/dev/cu.nonexistent"}}).wait();
    EXPECT_TRUE(result.is_error);
}

TEST_F(McpServerTest, ScriptRunningNotConnected) {
    auto result = client_->callTool("script_running", {{"cameraPath", "/dev/cu.nonexistent"}}).wait();
    EXPECT_TRUE(result.is_error);
}

TEST_F(McpServerTest, ReadFrameNotConnected) {
    auto result = client_->callTool("frame_capture", {{"cameraPath", "/dev/cu.nonexistent"}}).wait();
    EXPECT_TRUE(result.is_error);
}

TEST_F(McpServerTest, FrameEnableNotConnected) {
    auto result = client_->callTool("frame_enable", {{"cameraPath", "/dev/cu.nonexistent"}, {"enable", true}}).wait();
    EXPECT_TRUE(result.is_error);
}

TEST_F(McpServerTest, CameraResetNotConnected) {
    auto result = client_->callTool("camera_reset", {{"cameraPath", "/dev/cu.nonexistent"}}).wait();
    EXPECT_TRUE(result.is_error);
}

TEST_F(McpServerTest, ScriptSaveNotConnected) {
    auto result =
        client_->callTool("script_save", {{"cameraPath", "/dev/cu.nonexistent"}, {"script", "print('hello')"}}).wait();
    EXPECT_TRUE(result.is_error);
}
