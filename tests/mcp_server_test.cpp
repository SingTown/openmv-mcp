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
        server_->stopListening();
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
    EXPECT_EQ(tools.size(), 8u);
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

TEST_F(McpServerTest, CameraConnectInvalidPath) {
    auto resp = call_tool(60, "camera_connect", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, CameraDisconnectNotConnected) {
    auto resp = call_tool(61, "camera_disconnect", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, CameraInfoNotConnected) {
    auto resp = call_tool(62, "camera_info", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, ScriptRunningNotConnected) {
    auto resp = call_tool(63, "script_running", {{"cameraPath", "/dev/cu.nonexistent"}});
    ASSERT_TRUE(resp.contains("result"));
    EXPECT_TRUE(resp["result"].value("isError", false));
}

TEST_F(McpServerTest, ToolsListIncludesAllTools) {
    json req = {{"jsonrpc", "2.0"}, {"id", 64}, {"method", "tools/list"}};
    auto resp = post_mcp(req);
    ASSERT_TRUE(resp.contains("result"));
    auto& tools = resp["result"]["tools"];
    ASSERT_TRUE(tools.is_array());
    EXPECT_EQ(tools.size(), 8u);
    bool found_connect = false;
    bool found_disconnect = false;
    bool found_info = false;
    bool found_run = false;
    bool found_stop = false;
    bool found_terminal = false;
    bool found_script_running = false;
    for (const auto& t : tools) {
        if (t["name"] == "camera_connect") found_connect = true;
        if (t["name"] == "camera_disconnect") found_disconnect = true;
        if (t["name"] == "camera_info") found_info = true;
        if (t["name"] == "run_script") found_run = true;
        if (t["name"] == "stop_script") found_stop = true;
        if (t["name"] == "read_terminal") found_terminal = true;
        if (t["name"] == "script_running") found_script_running = true;
    }
    EXPECT_TRUE(found_connect);
    EXPECT_TRUE(found_disconnect);
    EXPECT_TRUE(found_info);
    EXPECT_TRUE(found_run);
    EXPECT_TRUE(found_stop);
    EXPECT_TRUE(found_terminal);
    EXPECT_TRUE(found_script_running);
}

// --- Device integration tests (auto-discover, skip if no camera) ---

static std::string discoverCamera(const std::function<json(int, const std::string&, const json&)>& call) {
    auto resp = call(100, "list_cameras", json::object());
    if (!resp.contains("result")) return "";
    auto& content = resp["result"]["content"];
    if (!content.is_array() || content.empty()) return "";
    auto cameras = json::parse(content[0]["text"].get<std::string>());
    if (!cameras.is_array() || cameras.empty()) return "";
    return cameras[0]["path"].get<std::string>();
}

TEST_F(McpServerTest, DeviceConnectAndInfo) {
    auto path = discoverCamera(call_tool);
    if (path.empty()) GTEST_SKIP() << "No camera connected";

    auto conn = call_tool(101, "camera_connect", {{"cameraPath", path}});
    ASSERT_FALSE(conn["result"].value("isError", false)) << conn.dump();

    auto info = call_tool(102, "camera_info", {{"cameraPath", path}});
    ASSERT_FALSE(info["result"].value("isError", false)) << info.dump();
    auto info_data = json::parse(info["result"]["content"][0]["text"].get<std::string>());
    EXPECT_FALSE(info_data["fwVersion"].get<std::string>().empty());
    std::cout << "Camera info: " << info_data.dump(2) << std::endl;

    call_tool(103, "camera_disconnect", {{"cameraPath", path}});
}

TEST_F(McpServerTest, DeviceRunScriptAndReadTerminal) {
    auto path = discoverCamera(call_tool);
    if (path.empty()) GTEST_SKIP() << "No camera connected";

    auto conn = call_tool(200, "camera_connect", {{"cameraPath", path}});
    ASSERT_TRUE(conn.contains("result")) << conn.dump();
    ASSERT_FALSE(conn["result"].value("isError", false)) << conn.dump();

    // Clear any existing terminal output
    call_tool(201, "read_terminal", {{"cameraPath", path}});

    // Run a simple print script
    auto run_resp = call_tool(202, "run_script", {{"cameraPath", path}, {"script", "print('hello_from_openmv')"}});
    ASSERT_FALSE(run_resp["result"].value("isError", false)) << run_resp.dump();

    // Poll read_terminal until we see the output
    std::string output;
    for (int i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = call_tool(210 + i, "read_terminal", {{"cameraPath", path}});
        if (resp.is_null() || !resp.contains("result")) continue;
        auto& text = resp["result"]["content"][0]["text"];
        if (text.is_null() || !text.is_string()) continue;
        auto data = json::parse(text.get<std::string>());
        output += data["output"].get<std::string>();
        if (output.find("hello_from_openmv") != std::string::npos) break;
    }

    std::cout << "Terminal output: [" << output << "]" << std::endl;
    EXPECT_NE(output.find("hello_from_openmv"), std::string::npos);

    call_tool(399, "camera_disconnect", {{"cameraPath", path}});
}

TEST_F(McpServerTest, DeviceStopScript) {
    auto path = discoverCamera(call_tool);
    if (path.empty()) GTEST_SKIP() << "No camera connected";

    auto conn = call_tool(400, "camera_connect", {{"cameraPath", path}});
    ASSERT_TRUE(conn.contains("result")) << conn.dump();
    ASSERT_FALSE(conn["result"].value("isError", false)) << conn.dump();

    // Run an infinite loop
    call_tool(
        401,
        "run_script",
        {{"cameraPath", path}, {"script", "import time\nwhile True:\n    print('running')\n    time.sleep_ms(100)\n"}});

    // Wait for some output
    std::string output;
    for (int i = 0; i < 50; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = call_tool(410 + i, "read_terminal", {{"cameraPath", path}});
        if (resp.is_null() || !resp.contains("result")) continue;
        auto& text = resp["result"]["content"][0]["text"];
        if (text.is_null() || !text.is_string()) continue;
        auto data = json::parse(text.get<std::string>());
        output += data["output"].get<std::string>();
        if (output.find("running") != std::string::npos) break;
    }
    std::cout << "Output before stop: [" << output << "]" << std::endl;
    EXPECT_NE(output.find("running"), std::string::npos);

    // Check script_running reports true while script is active
    auto sr_resp = call_tool(460, "script_running", {{"cameraPath", path}});
    ASSERT_FALSE(sr_resp["result"].value("isError", false)) << sr_resp.dump();
    auto sr_data = json::parse(sr_resp["result"]["content"][0]["text"].get<std::string>());
    EXPECT_TRUE(sr_data["running"].get<bool>()) << "script_running should be true while script is active";

    // Stop the script
    auto stop_resp = call_tool(470, "stop_script", {{"cameraPath", path}});
    ASSERT_TRUE(stop_resp.contains("result")) << stop_resp.dump();
    ASSERT_FALSE(stop_resp["result"].value("isError", false)) << stop_resp.dump();

    // Wait for poll to update script_running state
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check script_running reports false after stop
    auto sr_after = call_tool(473, "script_running", {{"cameraPath", path}});
    ASSERT_FALSE(sr_after["result"].value("isError", false)) << sr_after.dump();
    auto sr_after_data = json::parse(sr_after["result"]["content"][0]["text"].get<std::string>());
    EXPECT_FALSE(sr_after_data["running"].get<bool>()) << "script_running should be false after stop";

    // Clear and verify no new output
    call_tool(471, "read_terminal", {{"cameraPath", path}});

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto after_resp = call_tool(472, "read_terminal", {{"cameraPath", path}});
    std::string after_stop;
    if (!after_resp.is_null() && after_resp.contains("result")) {
        auto& text = after_resp["result"]["content"][0]["text"];
        if (text.is_string()) {
            auto after_data = json::parse(text.get<std::string>());
            after_stop = after_data["output"].get<std::string>();
        }
    }
    std::cout << "Output after stop: [" << after_stop << "]" << std::endl;
    EXPECT_EQ(after_stop.find("running"), std::string::npos);

    call_tool(499, "camera_disconnect", {{"cameraPath", path}});
}
