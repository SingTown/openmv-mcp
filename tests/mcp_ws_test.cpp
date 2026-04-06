#include "mcp_ws_test.h"

// =======================================================================
// WebSocket integration tests
// =======================================================================

TEST_F(DeviceTest, WebSocketScriptStatus) {
    WsReader reader("ws://127.0.0.1:" + std::to_string(kPort) + "/ws/script?camera=" + camera_path_);
    ASSERT_TRUE(reader.start()) << "WebSocket connect failed";

    EXPECT_TRUE(reader.waitForJson([](const json& j) { return j.contains("script_running"); }))
        << "Did not receive initial script status";

    call_tool("run_script",
              {{"cameraPath", camera_path_}, {"script", "import time\nwhile True:\n    time.sleep_ms(100)\n"}});

    EXPECT_TRUE(reader.waitForJson([](const json& j) { return j.value("script_running", false); }))
        << "Did not receive script_running=true";

    call_tool("stop_script", {{"cameraPath", camera_path_}});

    EXPECT_TRUE(reader.waitForJson([](const json& j) { return !j.value("script_running", true); }))
        << "Did not receive script_running=false";
}

TEST_F(DeviceTest, WebSocketTerminal) {
    WsReader reader("ws://127.0.0.1:" + std::to_string(kPort) + "/ws/terminal?camera=" + camera_path_);
    ASSERT_TRUE(reader.start()) << "WebSocket connect failed";

    call_tool("run_script", {{"cameraPath", camera_path_}, {"script", "print('ws_hello_test')"}});

    EXPECT_TRUE(reader.waitForString("ws_hello_test")) << "Did not receive expected terminal output";
}

TEST_F(DeviceTest, WebSocketFrame) {
    WsReader reader("ws://127.0.0.1:" + std::to_string(kPort) + "/ws/frame?camera=" + camera_path_);
    ASSERT_TRUE(reader.start()) << "WebSocket connect failed";

    call_tool("run_script", {{"cameraPath", camera_path_}, {"script", kSnapshotScript}});

    auto is_jpeg = [](const std::string& m) {
        return m.size() >= 2 && static_cast<uint8_t>(m[0]) == 0xFF && static_cast<uint8_t>(m[1]) == 0xD8;
    };

    ASSERT_TRUE(reader.waitFor(is_jpeg, 15000)) << "No JPEG frame received via WebSocket";
}

TEST_F(DeviceTest, ReadFrame) {
    auto run_resp = call_tool("run_script", {{"cameraPath", camera_path_}, {"script", kSnapshotScript}});
    ASSERT_FALSE(run_resp["result"].value("isError", false)) << run_resp.dump();

    json frame_content;
    std::string last_error;
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = call_tool("read_frame", {{"cameraPath", camera_path_}});
        if (resp.is_null() || !resp.contains("result")) continue;
        if (resp["result"].value("isError", false)) {
            auto& content = resp["result"]["content"];
            if (content.is_array() && !content.empty() && content[0].contains("text")) {
                last_error = content[0]["text"].get<std::string>();
            }
            continue;
        }
        auto& content = resp["result"]["content"];
        if (!content.is_array() || content.empty()) continue;
        if (content[0]["type"] != "image") continue;
        auto data = content[0]["data"].get<std::string>();
        if (!data.empty()) {
            frame_content = content[0];
            break;
        }
    }
    if (frame_content.is_null() && !last_error.empty()) {
        std::cout << "Last read_frame error: " << last_error << "\n";
    }

    ASSERT_FALSE(frame_content.is_null()) << "Failed to read a frame within timeout";
    EXPECT_EQ(frame_content["type"], "image");
    EXPECT_EQ(frame_content["mimeType"], "image/jpeg");
    EXPECT_FALSE(frame_content["data"].get<std::string>().empty());
    std::cout << "Frame base64 size: " << frame_content["data"].get<std::string>().size() << " bytes\n";
}
