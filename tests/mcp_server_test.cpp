#include "mcp_server_test.h"

// =======================================================================
// Protocol tests
// =======================================================================

TEST_F(McpServerTest, Health) {
    auto res = client_->Get("/health");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    auto body = json::parse(res->body);
    EXPECT_EQ(body["status"], "ok");
}

TEST_F(McpServerTest, Initialize) {
    json req = {{"jsonrpc", "2.0"},
                {"id", next_id_++},
                {"method", "initialize"},
                {"params",
                 {{"protocolVersion", "2025-03-26"},
                  {"capabilities", json::object()},
                  {"clientInfo", {{"name", "test"}, {"version", "1.0"}}}}}};
    auto resp = post_mcp(req);
    EXPECT_EQ(resp["jsonrpc"], "2.0");
    ASSERT_TRUE(resp.contains("result"));
    auto& r = resp["result"];
    EXPECT_EQ(r["protocolVersion"], "2025-03-26");
    EXPECT_TRUE(r.contains("capabilities"));
    EXPECT_EQ(r["serverInfo"]["name"], "openmv-mcp-server");
}

TEST_F(McpServerTest, Ping) {
    json req = {{"jsonrpc", "2.0"}, {"id", next_id_++}, {"method", "ping"}};
    auto resp = post_mcp(req);
    EXPECT_TRUE(resp.contains("result"));
}

TEST_F(McpServerTest, InvalidJson) {
    auto res = client_->Post("/mcp", "not json{{{", "application/json");
    ASSERT_NE(res, nullptr);
    auto resp = json::parse(res->body);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32700);
}

TEST_F(McpServerTest, InvalidJsonRpcVersion) {
    json req = {{"jsonrpc", "1.0"}, {"id", next_id_++}, {"method", "ping"}};
    auto resp = post_mcp(req);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32600);
}

TEST_F(McpServerTest, UnknownMethod) {
    json req = {{"jsonrpc", "2.0"}, {"id", next_id_++}, {"method", "nonexistent"}};
    auto resp = post_mcp(req);
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32601);
}

// =======================================================================
// Tool discovery & basic tool tests
// =======================================================================

TEST_F(McpServerTest, ToolsList) {
    json req = {{"jsonrpc", "2.0"}, {"id", next_id_++}, {"method", "tools/list"}};
    auto resp = post_mcp(req);
    ASSERT_TRUE(resp.contains("result"));
    auto& tools = resp["result"]["tools"];
    ASSERT_TRUE(tools.is_array());
    EXPECT_EQ(tools.size(), 10U);

    std::set<std::string> names;
    for (const auto& t : tools) names.insert(t["name"].get<std::string>());
    EXPECT_TRUE(names.count("list_cameras"));
    EXPECT_TRUE(names.count("camera_connect"));
    EXPECT_TRUE(names.count("camera_disconnect"));
    EXPECT_TRUE(names.count("camera_info"));
    EXPECT_TRUE(names.count("camera_reset"));
    EXPECT_TRUE(names.count("run_script"));
    EXPECT_TRUE(names.count("stop_script"));
    EXPECT_TRUE(names.count("read_terminal"));
    EXPECT_TRUE(names.count("script_running"));
    EXPECT_TRUE(names.count("read_frame"));
}

TEST_F(McpServerTest, UnknownTool) {
    auto resp = call_tool("nonexistent_tool");
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], -32602);
}

TEST_F(McpServerTest, ListCameras) {
    auto resp = call_tool("list_cameras");
    ASSERT_TRUE(resp.contains("result"));
    auto& content = resp["result"]["content"];
    ASSERT_TRUE(content.is_array());
    if (!content.empty()) {
        EXPECT_EQ(content[0]["type"], "text");
        auto cameras = json::parse(content[0]["text"].get<std::string>());
        EXPECT_TRUE(cameras.is_array());
    }
}

// =======================================================================
// Not-connected error tests
// =======================================================================

TEST_F(McpServerTest, CameraConnectInvalidPath) {
    auto resp = call_tool("camera_connect", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, CameraDisconnectNotConnected) {
    auto resp = call_tool("camera_disconnect", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, CameraInfoNotConnected) {
    auto resp = call_tool("camera_info", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, ScriptRunningNotConnected) {
    auto resp = call_tool("script_running", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, ReadFrameNotConnected) {
    auto resp = call_tool("read_frame", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, CameraResetNotConnected) {
    auto resp = call_tool("camera_reset", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}
