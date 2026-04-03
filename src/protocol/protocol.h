#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "serial_port/serial_port.h"

namespace mcp {

struct SystemInfo {
    uint32_t device_id[3] = {};
    uint32_t sensor_chip_id[3] = {};
    uint32_t capabilities = 0;
    uint32_t fw_version[3] = {};
    uint16_t vid = 0;
    uint16_t pid = 0;
    std::string board_type;
    std::string display_name;
};

class Protocol {
 public:
    virtual ~Protocol() = default;
    virtual void connect(std::shared_ptr<SerialPort> port) = 0;

    virtual void disconnect() {
        port_.reset();
        systemInfo = {};
    }

    [[nodiscard]] bool isConnected() const { return port_ && port_->isOpen(); }

    SystemInfo systemInfo;

 protected:
    void requireOpen() const {
        if (!port_ || !port_->isOpen()) throw std::runtime_error("Serial connection not open");
    }

    std::shared_ptr<SerialPort> port_;
};

}  // namespace mcp
