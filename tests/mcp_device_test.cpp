#include "mcp_device_test.h"

// =======================================================================
// Device integration tests
// =======================================================================

TEST_F(DeviceTest, ConnectAndInfo) {
    auto info = call_tool("camera_info", {{"cameraPath", camera_path_}});
    ASSERT_FALSE(info["result"].value("isError", false)) << info.dump();
    auto info_data = json::parse(info["result"]["content"][0]["text"].get<std::string>());
    EXPECT_FALSE(info_data["fwVersion"].get<std::string>().empty());
    std::cout << "Camera info: " << info_data.dump(2) << std::endl;
}

TEST_F(DeviceTest, RunScriptAndReadTerminal) {
    call_tool("read_terminal", {{"cameraPath", camera_path_}});

    auto run_resp = call_tool("run_script", {{"cameraPath", camera_path_}, {"script", "print('hello_from_openmv')"}});
    ASSERT_FALSE(run_resp["result"].value("isError", false)) << run_resp.dump();

    auto output = pollTerminalFor("hello_from_openmv");
    std::cout << "Terminal output: [" << output << "]" << std::endl;
    EXPECT_NE(output.find("hello_from_openmv"), std::string::npos);
}

TEST_F(DeviceTest, StopScript) {
    call_tool("run_script",
              {{"cameraPath", camera_path_},
               {"script", "import time\nwhile True:\n    print('running')\n    time.sleep_ms(100)\n"}});

    auto output = pollTerminalFor("running");
    std::cout << "Output before stop: [" << output << "]" << std::endl;
    EXPECT_NE(output.find("running"), std::string::npos);

    auto sr_resp = call_tool("script_running", {{"cameraPath", camera_path_}});
    ASSERT_FALSE(sr_resp["result"].value("isError", false)) << sr_resp.dump();
    auto sr_data = json::parse(sr_resp["result"]["content"][0]["text"].get<std::string>());
    EXPECT_TRUE(sr_data["running"].get<bool>()) << "script_running should be true while script is active";

    call_tool("stop_script", {{"cameraPath", camera_path_}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto sr_after = call_tool("script_running", {{"cameraPath", camera_path_}});
    ASSERT_FALSE(sr_after["result"].value("isError", false)) << sr_after.dump();
    auto sr_after_data = json::parse(sr_after["result"]["content"][0]["text"].get<std::string>());
    EXPECT_FALSE(sr_after_data["running"].get<bool>()) << "script_running should be false after stop";

    call_tool("read_terminal", {{"cameraPath", camera_path_}});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto after_resp = call_tool("read_terminal", {{"cameraPath", camera_path_}});
    std::string after_stop;
    if (!after_resp.is_null() && after_resp.contains("result")) {
        auto& text = after_resp["result"]["content"][0]["text"];
        if (text.is_string()) {
            after_stop = json::parse(text.get<std::string>())["output"].get<std::string>();
        }
    }
    std::cout << "Output after stop: [" << after_stop << "]" << std::endl;
    EXPECT_EQ(after_stop.find("running"), std::string::npos);
}
