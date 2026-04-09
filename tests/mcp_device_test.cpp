#include "mcp_device_test.h"

// =======================================================================
// Device integration tests
// =======================================================================

TEST_F(DeviceTest, ConnectAndInfo) {
    auto result = client_->callTool("camera_info", {{"cameraPath", camera_path_}}).wait();
    ASSERT_FALSE(result.is_error);
    auto info_data = json::parse(result.content[0].text);
    EXPECT_FALSE(info_data["fwVersion"].get<std::string>().empty());
    std::cout << "Camera info: " << info_data.dump(2) << std::endl;
}

TEST_F(DeviceTest, RunScriptAndReadTerminal) {
    client_->callTool("read_terminal", {{"cameraPath", camera_path_}}).wait();

    auto result =
        client_->callTool("run_script", {{"cameraPath", camera_path_}, {"script", "print('hello_from_openmv')"}})
            .wait();
    ASSERT_FALSE(result.is_error);

    auto output = pollTerminalFor("hello_from_openmv");
    std::cout << "Terminal output: [" << output << "]" << std::endl;
    EXPECT_NE(output.find("hello_from_openmv"), std::string::npos);
}

TEST_F(DeviceTest, StopScript) {
    client_
        ->callTool("run_script",
                   {{"cameraPath", camera_path_},
                    {"script", "import time\nwhile True:\n    print('running')\n    time.sleep_ms(100)\n"}})
        .wait();

    auto output = pollTerminalFor("running");
    std::cout << "Output before stop: [" << output << "]" << std::endl;
    EXPECT_NE(output.find("running"), std::string::npos);

    auto sr = client_->callTool("script_running", {{"cameraPath", camera_path_}}).wait();
    auto sr_data = json::parse(sr.content[0].text);
    EXPECT_TRUE(sr_data["running"].get<bool>()) << "script_running should be true while script is active";

    client_->callTool("stop_script", {{"cameraPath", camera_path_}}).wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto sr_after = client_->callTool("script_running", {{"cameraPath", camera_path_}}).wait();
    auto sr_after_data = json::parse(sr_after.content[0].text);
    EXPECT_FALSE(sr_after_data["running"].get<bool>()) << "script_running should be false after stop";

    client_->callTool("read_terminal", {{"cameraPath", camera_path_}}).wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto after_result = client_->callTool("read_terminal", {{"cameraPath", camera_path_}}).wait();
    std::string after_stop;
    if (!after_result.content.empty()) {
        auto terminal = json::parse(after_result.content[0].text);
        after_stop = terminal["output"].get<std::string>();
    }
    std::cout << "Output after stop: [" << after_stop << "]" << std::endl;
    EXPECT_EQ(after_stop.find("running"), std::string::npos);
}
