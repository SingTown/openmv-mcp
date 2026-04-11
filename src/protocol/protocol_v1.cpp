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
        readArchStr();
        readFwVersion();
        readSensorId();

        // Stop any running script
        delay(V1Const::SCRIPT_STOP_START_DELAY);
        sendCommand(V1Opcode::SCRIPT_STOP, 0);
        delay(V1Const::SCRIPT_STOP_END_DELAY);

        enableFrame(true);

        info.setProtocolVersion(1);
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
    info.setFwVersion(port_->read_le32(), port_->read_le32(), port_->read_le32());
}

void ProtocolV1::readArchStr() {
    sendCommand(V1Opcode::ARCH_STR, 64);
    auto arch_bytes = port_->read_bytes(64);
    std::string arch(arch_bytes.begin(), std::find(arch_bytes.begin(), arch_bytes.end(), static_cast<uint8_t>(0)));
    info = CameraInfo(arch);
}

void ProtocolV1::readSensorId() {
    sendCommand(V1Opcode::SENSOR_ID, 4);
    info.setSensorChipId({port_->read_le32(), 0, 0});
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
            if (bpp > 3) {
                // JPEG: bpp IS the compressed data size (not bytes-per-pixel)
                pixformat = PixFormat::JPEG;
                size = bpp;
            } else if (bpp == 3) {
                // BAYER: 1 byte per pixel
                pixformat = PixFormat::GRAYSCALE;
                size = w * h;
            } else if (bpp == 2) {
                pixformat = PixFormat::RGB565;
                size = w * h * 2;
            } else if (bpp == 1) {
                pixformat = PixFormat::GRAYSCALE;
                size = w * h;
            } else {
                pixformat = PixFormat::BINARY;
                size = ((w + 31) / 32) * 4 * h;
            }

            try {
                // Read frame in chunks — Windows CDC driver can't deliver large reads
                constexpr uint32_t kChunkSize = 4096;
                std::vector<uint8_t> frame_data;
                frame_data.reserve(size);
                uint32_t remaining = size;
                while (remaining > 0) {
                    uint32_t chunk = remaining < kChunkSize ? remaining : kChunkSize;
                    sendCommand(V1Opcode::FRAME_DUMP, chunk);
                    auto chunk_data = port_->read_bytes(chunk);
                    frame_data.insert(frame_data.end(), chunk_data.begin(), chunk_data.end());
                    remaining -= chunk;
                }
                setFrame(Frame(w, h, pixformat, std::move(frame_data)));
            } catch (...) {
                // Frame read failed — purge stale data and skip this frame
                // Don't rethrow: keep the poll loop alive
                port_->purge();
            }
        }
    }
}

}  // namespace mcp
