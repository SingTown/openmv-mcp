#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "serial_port/serial_port.h"

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

class Protocol {
 public:
    virtual ~Protocol() = default;
    virtual void connect(std::shared_ptr<SerialPort> port) = 0;
    virtual void disconnect();

    virtual void execScript(const std::string& script) = 0;
    virtual void stopScript() = 0;

    [[nodiscard]] std::string readTerminal();
    [[nodiscard]] bool isConnected() const { return port_ && port_->isOpen(); }
    [[nodiscard]] bool scriptRunning() const { return script_running_.load(); }

    SystemInfo systemInfo;

 protected:
    static constexpr int kPollMs = 10;

    virtual void poll() = 0;

    void startLoopThread();
    void stopLoopThread();

    void requireOpen() const {
        if (!port_ || !port_->isOpen()) throw std::runtime_error("Serial connection not open");
    }

    void appendTerminal(const std::string& data);

    static constexpr size_t kMaxTerminalBuf = size_t{1024} * 1024;
    std::shared_ptr<SerialPort> port_;
    std::thread loop_thread_;
    std::mutex io_mutex_;
    std::string terminal_buf_;
    std::atomic<bool> loop_running_{false};
    std::atomic<bool> script_running_{false};
};

}  // namespace mcp
