#pragma once

#include "mcp_server_test.h"

class DeviceTest : public McpServerTest {
 protected:
    void SetUp() override {
        camera_path_ = discoverCamera();
        if (camera_path_.empty()) GTEST_SKIP() << "No camera connected";

        auto conn = call_tool("camera_connect", {{"cameraPath", camera_path_}});
        ASSERT_TRUE(conn.contains("result")) << conn.dump();
        ASSERT_FALSE(conn["result"].value("isError", false)) << conn.dump();
    }

    void TearDown() override {
        if (camera_path_.empty()) return;
        call_tool("stop_script", {{"cameraPath", camera_path_}});
        call_tool("camera_disconnect", {{"cameraPath", camera_path_}});
    }

    static std::string discoverCamera() {
        auto resp = call_tool("list_cameras");
        if (!resp.contains("result")) return "";
        auto& content = resp["result"]["content"];
        if (!content.is_array() || content.empty()) return "";
        auto cameras = json::parse(content[0]["text"].get<std::string>());
        if (!cameras.is_array() || cameras.empty()) return "";
        return cameras[0]["path"].get<std::string>();
    }

    std::string pollTerminalFor(const std::string& expected, int timeout_ms = 5000) {
        std::string output;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto resp = call_tool("read_terminal", {{"cameraPath", camera_path_}});
            if (resp.is_null() || !resp.contains("result")) continue;
            auto& text = resp["result"]["content"][0]["text"];
            if (!text.is_string()) continue;
            output += json::parse(text.get<std::string>())["output"].get<std::string>();
            if (output.find(expected) != std::string::npos) break;
        }
        return output;
    }

    static inline const std::string kSnapshotScript =
        "import csi, time\n"
        "csi0 = csi.CSI()\n"
        "csi0.reset()\n"
        "csi0.pixformat(csi.RGB565)\n"
        "csi0.framesize(csi.QVGA)\n"
        "csi0.snapshot(time=2000)\n"
        "while True:\n"
        "    csi0.snapshot()\n"
        "    time.sleep_ms(100)\n";

    std::string camera_path_;
};
