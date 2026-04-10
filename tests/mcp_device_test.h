#pragma once

#include "mcp_server_test.h"

class DeviceTest : public McpServerTest {
 protected:
    void SetUp() override {
        camera_path_ = discoverCamera();
        if (camera_path_.empty()) GTEST_SKIP() << "No camera connected";

        auto result = client_->callTool("camera_connect", {{"cameraPath", camera_path_}}).wait();
        ASSERT_FALSE(result.is_error);
    }

    void TearDown() override {
        if (camera_path_.empty()) return;
        client_->callTool("script_stop", {{"cameraPath", camera_path_}}).wait();
        client_->callTool("camera_disconnect", {{"cameraPath", camera_path_}}).wait();
    }

    static std::string discoverCamera() {
        auto result = client_->callTool("camera_list").wait();
        if (result.content.empty()) return "";
        auto cameras = json::parse(result.content[0].text);
        if (!cameras.is_array() || cameras.empty()) return "";
        return cameras[0]["path"].get<std::string>();
    }

    std::string pollTerminalFor(const std::string& expected, int timeout_ms = 5000) {
        std::string output;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto result = client_->callTool("script_output", {{"cameraPath", camera_path_}}).wait();
            if (result.content.empty()) continue;
            auto terminal = json::parse(result.content[0].text);
            output += terminal["output"].get<std::string>();
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
