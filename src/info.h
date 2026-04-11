#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

#include "board.h"

namespace mcp {

class CameraInfo {
 public:
    CameraInfo() = default;
    CameraInfo(CameraInfo&& other) noexcept
        : device_id_(other.device_id_),
          sensor_chip_id_(other.sensor_chip_id_),
          capabilities_(other.capabilities_),
          fw_version_(other.fw_version_),
          protocol_version_(other.protocol_version_),
          board_(std::move(other.board_)),
          sensor_string_(std::move(other.sensor_string_)),
          camera_path_(std::move(other.camera_path_)),
          drive_path_(std::move(other.drive_path_)),
          licensed_(other.licensed_.load()) {}
    CameraInfo& operator=(CameraInfo&& other) noexcept {
        if (this != &other) {
            device_id_ = other.device_id_;
            sensor_chip_id_ = other.sensor_chip_id_;
            capabilities_ = other.capabilities_;
            fw_version_ = other.fw_version_;
            protocol_version_ = other.protocol_version_;
            board_ = std::move(other.board_);
            sensor_string_ = std::move(other.sensor_string_);
            camera_path_ = std::move(other.camera_path_);
            drive_path_ = std::move(other.drive_path_);
            licensed_.store(other.licensed_.load());
        }
        return *this;
    }
    CameraInfo(const CameraInfo&) = delete;
    CameraInfo& operator=(const CameraInfo&) = delete;

    explicit CameraInfo(uint16_t vid, uint16_t pid);
    explicit CameraInfo(const std::string& archString);

    void setSensorChipId(const std::array<uint32_t, 3>& ids);
    void setFwVersion(uint32_t major, uint32_t minor, uint32_t patch);
    void setDeviceId(const std::array<uint32_t, 3>& ids);
    void setCapabilities(uint32_t cap);
    void setProtocolVersion(uint32_t ver);
    void setCameraPath(const std::string& path);
    void checkLicense();
    void registerLicense(const std::string& boardKey);

    [[nodiscard]] uint16_t vid() const { return board_.vid; }
    [[nodiscard]] uint16_t pid() const { return board_.pid; }
    [[nodiscard]] const std::string& boardType() const { return board_.boardType; }
    [[nodiscard]] const std::string& boardName() const { return board_.name; }
    [[nodiscard]] const std::string& sensorString() const { return sensor_string_; }
    [[nodiscard]] const std::string& cameraPath() const { return camera_path_; }
    [[nodiscard]] const std::filesystem::path& drivePath() const { return drive_path_; }
    [[nodiscard]] std::string fwVersionString() const;
    [[nodiscard]] uint32_t capabilities() const { return capabilities_; }
    [[nodiscard]] uint32_t protocolVersion() const { return protocol_version_; }
    [[nodiscard]] bool licensed() const { return licensed_; }
    [[nodiscard]] std::string deviceIdHex() const;

 private:
    std::array<uint32_t, 3> device_id_ = {};
    std::array<uint32_t, 3> sensor_chip_id_ = {};
    uint32_t capabilities_ = 0;
    std::array<uint32_t, 3> fw_version_ = {};
    uint32_t protocol_version_ = 0;
    Board board_;
    std::string sensor_string_;
    std::string camera_path_;
    std::filesystem::path drive_path_;
    std::atomic<bool> licensed_ = true;
};

}  // namespace mcp
