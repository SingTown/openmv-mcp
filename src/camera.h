#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include "frame.h"
#include "serial_port/serial_port.h"
#include "utils/utf8_buffer.h"

namespace mcp {

struct SystemInfo {
    uint32_t device_id[3] = {};
    uint32_t sensor_chip_id[3] = {};
    uint32_t capabilities = 0;
    uint32_t fw_version[3] = {};
    uint16_t vid = 0;
    uint16_t pid = 0;
    uint32_t protocol_version = 0;
    std::string board_type;
    std::string display_name;
};

class Camera {
 public:
    virtual ~Camera() = default;

    static std::unique_ptr<Camera> create(const std::string& path);

    virtual void disconnect();

    virtual void execScript(const std::string& script) = 0;
    virtual void stopScript() = 0;
    virtual void enableFrame(bool enable) = 0;

    [[nodiscard]] std::string readTerminal();
    [[nodiscard]] std::optional<Frame> readFrame();
    [[nodiscard]] bool isConnected() const { return port_ && port_->isOpen(); }
    [[nodiscard]] bool scriptRunning() const { return script_running_.load(); }

    SystemInfo systemInfo;

 protected:
    static constexpr int kPollMs = 10;

    virtual void connect(std::shared_ptr<SerialPort> port) = 0;
    virtual void poll() = 0;

    void startLoopThread();
    void stopLoopThread();

    void requireOpen() const {
        if (!port_ || !port_->isOpen()) throw std::runtime_error("Serial connection not open");
    }

    std::shared_ptr<SerialPort> port_;
    std::thread loop_thread_;
    std::mutex io_mutex_;
    Utf8Buffer terminal_buf_;
    std::mutex frame_mutex_;
    std::optional<Frame> frame_;
    std::atomic<bool> loop_running_{false};
    std::atomic<bool> script_running_{false};
};

}  // namespace mcp
