#include "camera.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "board.h"
#include "protocol/protocol.h"
#include "protocol/protocol_v1.h"
#include "protocol/protocol_v2.h"
#include "serial_port/serial_port.h"
#include "utils/crc.h"

namespace mcp {

Camera::Camera() : port_(std::make_shared<SerialPort>()) {}

Camera::~Camera() {
    disconnect();
}

void Camera::connect(const std::string& path) {
    disconnect();

    if (!port_->open(path)) {
        throw std::runtime_error("Failed to open serial port: " + path);
    }

    int protoVersion = detectProtocol(*port_);
    if (protoVersion == 1) {
        protocol_ = std::make_unique<ProtocolV1>();
    } else {
        protocol_ = std::make_unique<ProtocolV2>();
    }
    try {
        protocol_->connect(port_);
    } catch (...) {
        protocol_.reset();
        port_->close();
        throw;
    }
}

void Camera::disconnect() {
    if (protocol_) {
        protocol_->disconnect();
        protocol_.reset();
    }
    port_->close();
}

bool Camera::isConnected() const {
    return protocol_ && protocol_->isConnected();
}

nlohmann::json Camera::systemInfo() const {
    if (!protocol_) return {};
    const auto& si = protocol_->systemInfo;

    char id_buf[25];
    std::snprintf(id_buf, sizeof(id_buf), "%08X%08X%08X", si.device_id[0], si.device_id[1], si.device_id[2]);

    std::string board_type = si.board_type;
    std::string display_name = si.display_name;
    if (board_type.empty() || display_name.empty()) {
        auto* b = findBoard(si.vid, si.pid);
        if (b) {
            if (board_type.empty()) board_type = b->boardType;
            if (display_name.empty()) display_name = b->displayName;
        }
    }

    char fw_buf[16];
    std::snprintf(fw_buf, sizeof(fw_buf), "%d.%d.%d", si.fw_version[0], si.fw_version[1], si.fw_version[2]);

    std::string sensor;
    for (int i = 0; i < 3; ++i) {
        if (si.sensor_chip_id[i] == 0) continue;
        auto it = ALL_SENSORS_MAP.find(si.sensor_chip_id[i]);
        if (it != ALL_SENSORS_MAP.end()) {
            if (!sensor.empty()) sensor += ", ";
            sensor += it->second;
        }
    }

    return {{"boardId", id_buf},
            {"boardType", board_type},
            {"displayName", display_name},
            {"sensor", sensor},
            {"fwVersion", fw_buf},
            {"protocolVersion", si.protocol_version}};
}

void Camera::execScript(const std::string& script) {
    protocol_->execScript(script);
}

void Camera::stopScript() {
    protocol_->stopScript();
}

std::string Camera::readTerminal() {
    return protocol_->readTerminal();
}

bool Camera::scriptRunning() const {
    if (!protocol_) return false;
    return protocol_->scriptRunning();
}

int Camera::detectProtocol(SerialPort& port) {
    // V1 probe: CMD + SENSOR_ID opcode
    uint8_t v1_cmd[6] = {0x30, 0x87, 0x04, 0x00, 0x00, 0x00};
    port.write_bytes({v1_cmd, v1_cmd + 6});

    // V2 probe: PROTO_SYNC packet with raw bytes (seq=0, chan=0, flags=0, opcode=0, len=4)
    uint8_t v2_hdr[8] = {0xAA, 0xD5, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00};
    uint8_t v2_payload[4] = {0, 0, 0, 0};
    port.write_bytes({v2_hdr, v2_hdr + 8});
    port.write_le16(crc16(v2_hdr, 8));
    port.write_bytes({v2_payload, v2_payload + 4});
    port.write_le32(crc32(v2_payload, 4));

    port.send();

    try {
        auto response = port.read_bytes(4);
        uint16_t first_word = static_cast<uint16_t>(response[0] | (response[1] << 8));
        return (first_word != 0xD5AA) ? 1 : 2;
    } catch (const std::runtime_error&) {
        throw std::runtime_error("Protocol detection failed: no response from device");
    }
}

}  // namespace mcp
