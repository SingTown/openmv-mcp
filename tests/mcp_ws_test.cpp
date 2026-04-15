#include "mcp_ws_test.h"

// =======================================================================
// WebSocket integration tests
// =======================================================================

TEST_F(DeviceTest, WebSocketStatus) {
    WsReader reader("ws://127.0.0.1:" + std::to_string(kPort) + "/ws/status?camera=" + camera_path_);
    ASSERT_TRUE(reader.start()) << "WebSocket connect failed";

    EXPECT_TRUE(reader.waitForJson([](const json& j) {
        return j.contains("connected") && j.contains("script_running");
    })) << "Did not receive initial status";

    client_
        ->callTool("script_run",
                   {{"cameraPath", camera_path_}, {"script", "import time\nwhile True:\n    time.sleep_ms(100)\n"}})
        .wait();

    EXPECT_TRUE(reader.waitForJson([](const json& j) { return j.value("script_running", false); }))
        << "Did not receive script_running=true";

    client_->callTool("script_stop", {{"cameraPath", camera_path_}}).wait();

    EXPECT_TRUE(reader.waitForJson([](const json& j) { return !j.value("script_running", true); }))
        << "Did not receive script_running=false";
}

TEST_F(DeviceTest, WebSocketTerminal) {
    WsReader reader("ws://127.0.0.1:" + std::to_string(kPort) + "/ws/terminal?camera=" + camera_path_);
    ASSERT_TRUE(reader.start()) << "WebSocket connect failed";

    client_->callTool("script_run", {{"cameraPath", camera_path_}, {"script", "print('ws_hello_test')"}}).wait();

    EXPECT_TRUE(reader.waitForString("ws_hello_test")) << "Did not receive expected terminal output";
}

TEST_F(DeviceTest, HttpFrameStream) {
    client_->callTool("script_run", {{"cameraPath", camera_path_}, {"script", kSnapshotScript}}).wait();

    httplib::Client http("127.0.0.1", kPort);
    http.set_read_timeout(15, 0);

    std::atomic<bool> got_jpeg{false};
    std::string buf;
    auto res = http.Get("/stream/frame?camera=" + camera_path_, [&](const char* data, size_t len) {
        buf.append(data, len);
        for (size_t i = 0; i + 1 < buf.size(); ++i) {
            if (static_cast<uint8_t>(buf[i]) == 0xFF && static_cast<uint8_t>(buf[i + 1]) == 0xD8) {
                got_jpeg.store(true);
                return false;
            }
        }
        return true;
    });

    ASSERT_TRUE(got_jpeg.load()) << "No JPEG frame received via HTTP MJPEG stream";
}

TEST_F(DeviceTest, ReadFrame) {
    auto run_result =
        client_->callTool("script_run", {{"cameraPath", camera_path_}, {"script", kSnapshotScript}}).wait();
    ASSERT_FALSE(run_result.is_error);

    mcp::ContentItem frame_content;
    std::string last_error;
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto result = client_->callTool("frame_capture", {{"cameraPath", camera_path_}}).wait();
        if (result.content.empty()) continue;
        if (result.is_error) {
            last_error = result.content[0].text;
            continue;
        }
        if (result.content[0].type != "image") continue;
        if (!result.content[0].data.empty()) {
            frame_content = result.content[0];
            break;
        }
    }
    if (frame_content.type.empty() && !last_error.empty()) {
        std::cout << "Last read_frame error: " << last_error << "\n";
    }

    ASSERT_FALSE(frame_content.type.empty()) << "Failed to read a frame within timeout";
    EXPECT_EQ(frame_content.type, "image");
    EXPECT_EQ(frame_content.mime_type, "image/jpeg");
    EXPECT_FALSE(frame_content.data.empty());
    std::cout << "Frame base64 size: " << frame_content.data.size() << " bytes\n";
}
