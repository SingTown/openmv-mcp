#include "protocol_v1.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

#include "board.h"
#include "serial_port/serial_port.h"

namespace mcp {

namespace V1Opcode {
constexpr uint8_t CMD = 0x30;
constexpr uint8_t SCRIPT_EXEC = 0x05;
constexpr uint8_t SCRIPT_STOP = 0x06;
constexpr uint8_t FW_VERSION = 0x80;
constexpr uint8_t ARCH_STR = 0x83;
constexpr uint8_t SENSOR_ID = 0x90;
constexpr uint8_t SYS_RESET = 0x0C;
constexpr uint8_t SYS_RESET_TO_BL = 0x0E;
constexpr uint8_t FB_ENABLE = 0x0D;
constexpr uint8_t GET_STATE = 0x93;
constexpr uint8_t FRAME_DUMP = 0x82;
}  // namespace V1Opcode

namespace V1Const {
constexpr uint32_t TABOO_PACKET_SIZE = 64;
// Per-command delays matching OpenMV IDE (openmvpluginserialport.h)
constexpr int SYS_RESET_START_DELAY = 50;
constexpr int SYS_RESET_END_DELAY = 100;
constexpr int SCRIPT_EXEC_START_DELAY = 50;
constexpr int SCRIPT_EXEC_END_DELAY = 25;
constexpr int SCRIPT_EXEC_2_START_DELAY = 25;
constexpr int SCRIPT_EXEC_2_END_DELAY = 50;
constexpr int SCRIPT_STOP_START_DELAY = 50;
constexpr int SCRIPT_STOP_END_DELAY = 50;
constexpr uint32_t FB_ENABLE_PAYLOAD_LEN = 4;
constexpr int FB_ENABLE_0_START_DELAY = 50;
constexpr int FB_ENABLE_0_END_DELAY = 25;
constexpr int FB_ENABLE_1_START_DELAY = 25;
constexpr int FB_ENABLE_1_END_DELAY = 50;
constexpr int FRAME_DUMP_START_DELAY = 50;
constexpr int FRAME_DUMP_END_DELAY = 25;
}  // namespace V1Const

void ProtocolV1::delay(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void ProtocolV1::sendCommand(uint8_t opcode, uint32_t len) {
    requireOpen();
    port_->write_u8(V1Opcode::CMD);
    port_->write_u8(opcode);
    port_->write_le32(len);
    if (!port_->send()) throw std::runtime_error("Failed to send command");
}

void ProtocolV1::connect(std::shared_ptr<SerialPort> port) {
    disconnect();
    port_ = std::move(port);

    requireOpen();

    try {
        readFwVersion();

        // GET_STATE (0x93) requires firmware >= 4.5.6
        auto& v = systemInfo.fw_version;
        if (std::make_tuple(v[0], v[1], v[2]) < std::make_tuple(4U, 5U, 6U)) {
            throw std::runtime_error("Firmware version " + std::to_string(v[0]) + "." + std::to_string(v[1]) + "." +
                                     std::to_string(v[2]) + " is too old, requires >= 4.5.6");
        }

        readArchStr();
        readSensorId();

        // Stop any running script
        delay(V1Const::SCRIPT_STOP_START_DELAY);
        sendCommand(V1Opcode::SCRIPT_STOP, 0);
        delay(V1Const::SCRIPT_STOP_END_DELAY);

        enableFrame(true);

        systemInfo.protocol_version = 1;
        startLoopThread();
    } catch (...) {
        disconnect();
        throw;
    }
}

void ProtocolV1::reset() {
    requireOpen();

    // Stop loop thread first so it won't poll a resetting device
    stopLoopThread();

    delay(V1Const::SYS_RESET_START_DELAY);
    sendCommand(V1Opcode::SYS_RESET, 0);
    delay(V1Const::SYS_RESET_END_DELAY);

    disconnect();
}

void ProtocolV1::boot() {
    requireOpen();

    // Stop loop thread first so it won't poll a rebooting device
    stopLoopThread();

    delay(V1Const::SYS_RESET_START_DELAY);
    sendCommand(V1Opcode::SYS_RESET_TO_BL, 0);
    delay(V1Const::SYS_RESET_END_DELAY);

    disconnect();
}

void ProtocolV1::readFwVersion() {
    sendCommand(V1Opcode::FW_VERSION, 12);
    for (auto& v : systemInfo.fw_version) v = port_->read_le32();
}

void ProtocolV1::readArchStr() {
    sendCommand(V1Opcode::ARCH_STR, 64);
    auto arch_bytes = port_->read_bytes(64);
    std::string arch(arch_bytes.begin(), std::find(arch_bytes.begin(), arch_bytes.end(), static_cast<uint8_t>(0)));

    // Parse arch string: "OMV4 H7 1024 [H7:AABBCCDD11223344EEFF5566]"
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

void ProtocolV1::readSensorId() {
    sendCommand(V1Opcode::SENSOR_ID, 4);
    systemInfo.sensor_chip_id[0] = port_->read_le32();
}

void ProtocolV1::execScript(const std::string& script) {
    requireOpen();

    std::string data = script;
    // Avoid USB ZLP (Zero-Length Packet) issue on 64-byte boundaries
    if (data.size() % V1Const::TABOO_PACKET_SIZE == 0) {
        data.push_back('\n');
    }

    std::lock_guard<std::mutex> lock(io_mutex_);
    delay(V1Const::SCRIPT_EXEC_START_DELAY);
    sendCommand(V1Opcode::SCRIPT_EXEC, static_cast<uint32_t>(data.size()));
    delay(V1Const::SCRIPT_EXEC_END_DELAY);
    delay(V1Const::SCRIPT_EXEC_2_START_DELAY);
    port_->write_bytes(std::vector<uint8_t>(data.begin(), data.end()));
    if (!port_->send()) throw std::runtime_error("Failed to send script data");
    delay(V1Const::SCRIPT_EXEC_2_END_DELAY);
}

void ProtocolV1::stopScript() {
    requireOpen();

    std::lock_guard<std::mutex> lock(io_mutex_);
    delay(V1Const::SCRIPT_STOP_START_DELAY);
    sendCommand(V1Opcode::SCRIPT_STOP, 0);
    delay(V1Const::SCRIPT_STOP_END_DELAY);
}

void ProtocolV1::enableFrame(bool enable) {
    requireOpen();

    std::lock_guard<std::mutex> lock(io_mutex_);

    // Breakup mode (firmware >= 3.5.3): send header and payload separately
    delay(V1Const::FB_ENABLE_0_START_DELAY);
    sendCommand(V1Opcode::FB_ENABLE, V1Const::FB_ENABLE_PAYLOAD_LEN);
    delay(V1Const::FB_ENABLE_0_END_DELAY);

    delay(V1Const::FB_ENABLE_1_START_DELAY);
    port_->write_le32(enable ? 1 : 0);
    if (!port_->send()) throw std::runtime_error("Failed to send FB_ENABLE payload");
    delay(V1Const::FB_ENABLE_1_END_DELAY);
}

void ProtocolV1::poll() {
    std::lock_guard<std::mutex> lock(io_mutex_);

    sendCommand(V1Opcode::GET_STATE, V1State::kPayloadLen);
    V1State state(port_->read_bytes(V1State::kPayloadLen));

    updateScript(state.scriptRunning());

    if (state.hasText() && !state.textData().empty()) {
        appendTerminal(state.textData());
    }

    if (state.hasFrame()) {
        uint32_t w = state.frameWidth();
        uint32_t h = state.frameHeight();
        uint32_t bpp = state.frameBpp();

        if (w > 0 && h > 0) {
            uint32_t pixformat = 0;
            uint32_t size = 0;
            if (bpp >= 3) {
                pixformat = PixFormat::JPEG;
                size = w * h * bpp;
            } else if (bpp == 2) {
                pixformat = PixFormat::RGB565;
                size = w * h * 2;
            } else if (bpp == 1) {
                pixformat = PixFormat::GRAYSCALE;
                size = w * h;
            } else {
                pixformat = PixFormat::BINARY;
                size = ((w + 7) / 8) * h;
            }

            delay(V1Const::FRAME_DUMP_START_DELAY);
            sendCommand(V1Opcode::FRAME_DUMP, size);
            delay(V1Const::FRAME_DUMP_END_DELAY);
            auto data = port_->read_bytes(size);

            setFrame(Frame(w, h, pixformat, std::move(data)));
        }
    }
}

}  // namespace mcp
