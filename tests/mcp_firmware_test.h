#pragma once

#include "mcp_server_test.h"

class FirmwareTest : public McpServerTest {
 public:
    static std::string discoverCamera() {
        auto result = client_->callTool("camera_list").wait();
        if (result.content.empty()) return "";
        auto cameras = json::parse(result.content[0].text);
        if (!cameras.is_array() || cameras.empty()) return "";
        return cameras[0]["path"].get<std::string>();
    }

    void SetUp() override {
        camera_path_ = discoverCamera();
        if (camera_path_.empty()) GTEST_SKIP() << "No camera connected";

        auto conn = client_->callTool("camera_connect", {{"cameraPath", camera_path_}});
        auto conn_result = conn.wait();
        ASSERT_FALSE(conn_result.is_error);

        auto info = client_->callTool("camera_info", {{"cameraPath", camera_path_}});
        auto info_result = info.wait();
        ASSERT_FALSE(info_result.is_error);
        auto info_data = json::parse(info_result.content[0].text);
        board_name_ = info_data["name"].get<std::string>();
    }

    void TearDown() override {
        if (!camera_path_.empty()) {
            auto resp = client_->callTool("camera_disconnect", {{"cameraPath", camera_path_}});
            resp.wait();
        }
    }

    std::string waitAndReconnect(int timeout_s = 30) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto path = discoverCamera();
            if (!path.empty()) {
                camera_path_ = path;
                auto conn = client_->callTool("camera_connect", {{"cameraPath", camera_path_}});
                auto conn_result = conn.wait();
                if (!conn_result.is_error) {
                    auto info = client_->callTool("camera_info", {{"cameraPath", camera_path_}});
                    auto info_result = info.wait();
                    if (!info_result.is_error) {
                        auto data = json::parse(info_result.content[0].text);
                        return data["fwVersion"].get<std::string>();
                    }
                }
            }
        }
        return "";
    }

    std::string camera_path_;
    std::string board_name_;
};
