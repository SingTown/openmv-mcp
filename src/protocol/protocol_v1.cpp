#include "protocol_v1.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include "board.h"
#include "serial_port/serial_port.h"

namespace mcp {

namespace V1Opcode {
constexpr uint8_t CMD = 0x30;
constexpr uint8_t FW_VERSION = 0x80;
constexpr uint8_t ARCH_STR = 0x83;
constexpr uint8_t SENSOR_ID = 0x90;
}  // namespace V1Opcode

namespace V1Const {
constexpr int FW_VERSION_LEN = 12;
constexpr int ARCH_STR_LEN = 64;
constexpr int SENSOR_ID_LEN = 4;
}  // namespace V1Const

void ProtocolV1::sendCommand(uint8_t opcode, uint32_t len) {
    requireOpen();
    port_->write_u8(V1Opcode::CMD);
    port_->write_u8(opcode);
    port_->write_le32(len);
    if (!port_->send()) throw std::runtime_error("Write failed");
}

void ProtocolV1::connect(std::shared_ptr<SerialPort> port) {
    disconnect();
    port_ = std::move(port);

    requireOpen();

    sendCommand(V1Opcode::FW_VERSION, V1Const::FW_VERSION_LEN);
    systemInfo.fw_version[0] = port_->read_le32();
    systemInfo.fw_version[1] = port_->read_le32();
    systemInfo.fw_version[2] = port_->read_le32();

    // Arch string format: "OMV4 H7 1024 [H7:AABBCCDD11223344EEFF5566]"
    sendCommand(V1Opcode::ARCH_STR, V1Const::ARCH_STR_LEN);
    auto archData = port_->read_bytes(V1Const::ARCH_STR_LEN);
    {
        std::string arch(reinterpret_cast<const char*>(archData.data()), V1Const::ARCH_STR_LEN);
        arch = arch.c_str();
        auto lbracket = arch.rfind('[');
        auto colon = arch.rfind(':');
        auto rbracket = arch.rfind(']');
        if (lbracket != std::string::npos && colon != std::string::npos && rbracket != std::string::npos &&
            colon > lbracket && rbracket > colon) {
            systemInfo.board_type = arch.substr(lbracket + 1, colon - lbracket - 1);
            std::string archPrefix = arch.substr(0, lbracket);
            while (!archPrefix.empty() && archPrefix.back() == ' ') archPrefix.pop_back();
            for (const auto& b : ALL_BOARDS) {
                if (b.archString == archPrefix) {
                    systemInfo.display_name = b.displayName;
                    break;
                }
            }
            std::string idHex = arch.substr(colon + 1, rbracket - colon - 1);
            if (idHex.size() == 24) {
                for (int i = 0; i < 3; i++) {
                    systemInfo.device_id[i] = static_cast<uint32_t>(
                        std::strtoul(idHex.substr(static_cast<size_t>(i) * 8, 8).c_str(), nullptr, 16));
                }
            }
        }
    }

    sendCommand(V1Opcode::SENSOR_ID, V1Const::SENSOR_ID_LEN);
    systemInfo.sensor_chip_id[0] = port_->read_le32();
}

}  // namespace mcp
