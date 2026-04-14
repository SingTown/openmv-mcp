#include "mcp_device_test.h"

#include <filesystem>
#include <fstream>

// =======================================================================
// Device integration tests
// =======================================================================

TEST_F(DeviceTest, ConnectAndInfo) {
    auto result = client_->callTool("camera_info", {{"cameraPath", camera_path_}}).wait();
    ASSERT_FALSE(result.is_error);
    auto info_data = json::parse(result.content[0].text);
    EXPECT_FALSE(info_data["fwVersion"].get<std::string>().empty());
    EXPECT_TRUE(info_data.contains("cameraPath"));
    EXPECT_TRUE(info_data.contains("drivePath"));
    EXPECT_EQ(info_data["cameraPath"].get<std::string>(), camera_path_);
    EXPECT_FALSE(info_data["drivePath"].get<std::string>().empty())
        << "drivePath should not be empty for a connected camera";
    std::cout << "Camera info: " << info_data.dump(2) << std::endl;
}

TEST_F(DeviceTest, RunScriptAndReadTerminal) {
    client_->callTool("script_output", {{"cameraPath", camera_path_}}).wait();

    auto result =
        client_->callTool("script_run", {{"cameraPath", camera_path_}, {"script", "print('hello_from_openmv')"}})
            .wait();
    ASSERT_FALSE(result.is_error);

    auto output = pollTerminalFor("hello_from_openmv");
    std::cout << "Terminal output: [" << output << "]" << std::endl;
    EXPECT_NE(output.find("hello_from_openmv"), std::string::npos);
}

TEST_F(DeviceTest, ScriptRunningAfterDelay) {
    // Run a snapshot script that also prints to terminal
    auto result = client_
                      ->callTool("script_run",
                                 {{"cameraPath", camera_path_},
                                  {"script", kSnapshotScript + std::string("    print('frame_ok')\n")}})
                      .wait();
    ASSERT_FALSE(result.is_error);

    // Wait for first frame to be ready
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // Check every second for 10 seconds: frame readable + terminal output
    int frame_ok = 0;
    int terminal_ok = 0;
    for (int i = 0; i < 10; i++) {
        auto frame = client_->callTool("frame_capture", {{"cameraPath", camera_path_}}).wait();
        if (!frame.is_error) frame_ok++;

        auto term = client_->callTool("script_output", {{"cameraPath", camera_path_}}).wait();
        if (!term.content.empty()) {
            auto terminal = json::parse(term.content[0].text);
            std::string output = terminal["output"].get<std::string>();
            if (output.find("frame_ok") != std::string::npos) terminal_ok++;
        }

        auto sr = client_->callTool("script_running", {{"cameraPath", camera_path_}}).wait();
        auto sr_data = json::parse(sr.content[0].text);
        EXPECT_TRUE(sr_data["running"].get<bool>()) << "script stopped at second " << i;

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    std::cout << "Frames read: " << frame_ok << "/10, Terminal reads: " << terminal_ok << "/10" << std::endl;
    EXPECT_GE(frame_ok, 5) << "Should read frames in at least half of the attempts";
    EXPECT_GE(terminal_ok, 5) << "Should read terminal in at least half of the attempts";
}

TEST_F(DeviceTest, FrameEnableToggle) {
    auto run = client_->callTool("script_run", {{"cameraPath", camera_path_}, {"script", kSnapshotScript}}).wait();
    ASSERT_FALSE(run.is_error);

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    auto frame_on = client_->callTool("frame_capture", {{"cameraPath", camera_path_}}).wait();
    EXPECT_FALSE(frame_on.is_error) << "frame should be available when enabled";

    auto disable = client_->callTool("frame_enable", {{"cameraPath", camera_path_}, {"enable", false}}).wait();
    ASSERT_FALSE(disable.is_error);
    EXPECT_TRUE(json::parse(disable.content[0].text)["success"].get<bool>());

    client_->callTool("frame_capture", {{"cameraPath", camera_path_}}).wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    int disabled_ok = 0;
    for (int i = 0; i < 5; i++) {
        auto f = client_->callTool("frame_capture", {{"cameraPath", camera_path_}}).wait();
        if (!f.is_error) disabled_ok++;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    std::cout << "Frames while disabled: " << disabled_ok << "/5" << std::endl;
    EXPECT_LE(disabled_ok, 1) << "at most one in-flight frame may arrive after disable";

    auto enable = client_->callTool("frame_enable", {{"cameraPath", camera_path_}, {"enable", true}}).wait();
    ASSERT_FALSE(enable.is_error);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    int enabled_ok = 0;
    for (int i = 0; i < 5; i++) {
        auto f = client_->callTool("frame_capture", {{"cameraPath", camera_path_}}).wait();
        if (!f.is_error) enabled_ok++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "Frames after re-enable: " << enabled_ok << "/5" << std::endl;
    EXPECT_GE(enabled_ok, 3) << "frames should resume after re-enabling";
}

TEST_F(DeviceTest, StopScript) {
    client_
        ->callTool("script_run",
                   {{"cameraPath", camera_path_},
                    {"script", "import time\nwhile True:\n    print('running')\n    time.sleep_ms(100)\n"}})
        .wait();

    auto output = pollTerminalFor("running");
    std::cout << "Output before stop: [" << output << "]" << std::endl;
    EXPECT_NE(output.find("running"), std::string::npos);

    auto sr = client_->callTool("script_running", {{"cameraPath", camera_path_}}).wait();
    auto sr_data = json::parse(sr.content[0].text);
    EXPECT_TRUE(sr_data["running"].get<bool>()) << "script_running should be true while script is active";

    client_->callTool("script_stop", {{"cameraPath", camera_path_}}).wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto sr_after = client_->callTool("script_running", {{"cameraPath", camera_path_}}).wait();
    auto sr_after_data = json::parse(sr_after.content[0].text);
    EXPECT_FALSE(sr_after_data["running"].get<bool>()) << "script_running should be false after stop";

    client_->callTool("script_output", {{"cameraPath", camera_path_}}).wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto after_result = client_->callTool("script_output", {{"cameraPath", camera_path_}}).wait();
    std::string after_stop;
    if (!after_result.content.empty()) {
        auto terminal = json::parse(after_result.content[0].text);
        after_stop = terminal["output"].get<std::string>();
    }
    std::cout << "Output after stop: [" << after_stop << "]" << std::endl;
    EXPECT_EQ(after_stop.find("running"), std::string::npos);
}

TEST_F(DeviceTest, ScriptSave) {
    // Get drive path from camera info
    auto info_result = client_->callTool("camera_info", {{"cameraPath", camera_path_}}).wait();
    ASSERT_FALSE(info_result.is_error);
    auto info_data = json::parse(info_result.content[0].text);
    auto drive_path = info_data["drivePath"].get<std::string>();
    ASSERT_FALSE(drive_path.empty()) << "drivePath must not be empty for script_save test";

    // Save a script to main.py
    const std::string script = "# script_save test\nprint('saved_ok')\n";
    auto result = client_->callTool("script_save", {{"cameraPath", camera_path_}, {"script", script}}).wait();
    ASSERT_FALSE(result.is_error);
    auto resp = json::parse(result.content[0].text);
    EXPECT_TRUE(resp["success"].get<bool>());
    EXPECT_FALSE(resp["path"].get<std::string>().empty());

    // Verify file was actually written to disk
    auto filepath = std::filesystem::path(drive_path) / "main.py";
    ASSERT_TRUE(std::filesystem::exists(filepath)) << "main.py should exist on the USB drive";

    std::ifstream ifs(filepath);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, script);
    std::cout << "Saved main.py at: " << filepath << std::endl;
}
