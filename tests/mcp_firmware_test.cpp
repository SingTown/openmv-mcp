#include "mcp_firmware_test.h"

#include <iostream>

// =======================================================================
// Firmware streaming tool error tests (no device needed)
// =======================================================================

TEST_F(McpServerTest, FlashFirmwareNotConnected) {
    auto resp = client_->callTool("firmware_flash", {{"cameraPath", "/dev/cu.nonexistent"}});
    auto result = resp.wait();
    EXPECT_TRUE(result.is_error);
}

// =======================================================================
// Firmware cancel tests (no device needed)
// =======================================================================

TEST_F(McpServerTest, CancelFirmwareRepair) {
    auto resp = client_->callTool("firmware_repair", {{"name", "OpenMV Cam N6"}});

    std::this_thread::sleep_for(std::chrono::seconds(3));

    client_->cancelTool(resp.requestId());

    auto result = resp.wait();

    EXPECT_TRUE(result.is_error);
}

// =======================================================================
// Firmware integration tests (require real device, long running)
// =======================================================================

TEST_F(FirmwareTest, UpgradeFirmware) {
    std::cout << "Upgrading firmware for: " << board_name_ << '\n';

    auto resp = client_->callTool("firmware_flash", {{"cameraPath", camera_path_}});
    auto result = resp.wait();
    ASSERT_FALSE(result.is_error) << result.content[0].text;
    auto result_data = json::parse(result.content.back().text);
    EXPECT_TRUE(result_data["success"].get<bool>());

    std::cout << "Upgrade result: " << result_data.dump() << '\n';

    auto fwVersion = waitAndReconnect();
    EXPECT_FALSE(fwVersion.empty()) << "Device did not reappear after upgrade";
    std::cout << "Firmware version after upgrade: " << fwVersion << '\n';
}
