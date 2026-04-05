#include "protocol_v2.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>

#include "serial_port/serial_port.h"
#include "utils/crc.h"

namespace mcp {

namespace Flags {
constexpr uint8_t ACK = 1 << 0;
constexpr uint8_t NAK = 1 << 1;
constexpr uint8_t RTX = 1 << 2;
constexpr uint8_t ACK_REQ = 1 << 3;
constexpr uint8_t FRAGMENT = 1 << 4;
constexpr uint8_t EVENT = 1 << 5;
}  // namespace Flags

namespace Proto {
constexpr uint16_t SYNC_WORD = 0xD5AA;
constexpr uint16_t HEADER_SIZE = 10;
constexpr uint16_t CRC_SIZE = 4;
constexpr uint16_t MIN_PAYLOAD_SIZE = 52;
}  // namespace Proto

namespace Status {
constexpr uint8_t BUSY = 0x04;
constexpr uint8_t CHECKSUM = 0x05;
constexpr uint8_t SEQUENCE = 0x06;
constexpr uint8_t TIMEOUT = 0x03;
}  // namespace Status

namespace Opcode {
constexpr uint8_t PROTO_SYNC = 0x00;
constexpr uint8_t PROTO_GET_CAPS = 0x01;
constexpr uint8_t PROTO_SET_CAPS = 0x02;
constexpr uint8_t PROTO_VERSION = 0x04;
constexpr uint8_t SYS_RESET = 0x10;
constexpr uint8_t SYS_BOOT = 0x11;
constexpr uint8_t SYS_INFO = 0x12;
constexpr uint8_t CHANNEL_LIST = 0x20;
constexpr uint8_t CHANNEL_POLL = 0x21;
constexpr uint8_t CHANNEL_LOCK = 0x22;
constexpr uint8_t CHANNEL_UNLOCK = 0x23;
constexpr uint8_t CHANNEL_SIZE = 0x25;
constexpr uint8_t CHANNEL_READ = 0x26;
constexpr uint8_t CHANNEL_WRITE = 0x27;
constexpr uint8_t CHANNEL_IOCTL = 0x28;
}  // namespace Opcode

namespace StdinIoctl {
constexpr uint32_t STOP = 0x01;
constexpr uint32_t EXEC = 0x02;
constexpr uint32_t RESET = 0x03;
}  // namespace StdinIoctl

namespace EventType {
constexpr uint8_t CHANNEL_REGISTERED = 0x00;
constexpr uint8_t CHANNEL_UNREGISTERED = 0x01;
constexpr uint8_t SOFT_REBOOT = 0x02;
}  // namespace EventType

void ProtocolV2::sendPacket(const Packet& pkt) {
    requireOpen();
    if (pkt.payload.size() > max_payload_) throw std::runtime_error("Payload too large");

    port_->write_bytes({0xAA, 0xD5, pkt.sequence, pkt.channel, pkt.flags, pkt.opcode});
    port_->write_le16(pkt.length);
    port_->write_le16(pkt.hdrCrc());

    if (!pkt.payload.empty()) {
        port_->write_bytes(pkt.payload);
        port_->write_le32(pkt.payloadCrc());
    }

    if (!port_->send()) throw std::runtime_error("Failed to write packet");
}

Packet ProtocolV2::readPacket() {
    constexpr int kMaxRetries = 10;
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
        uint8_t prev = 0;
        while (true) {
            uint8_t b = port_->read_u8();
            if (prev == (Proto::SYNC_WORD & 0xFF) && b == ((Proto::SYNC_WORD >> 8) & 0xFF)) break;
            prev = b;
        }

        uint8_t seq = port_->read_u8();
        uint8_t chan = port_->read_u8();
        uint8_t flags = port_->read_u8();
        uint8_t opcode = port_->read_u8();
        uint16_t length = port_->read_le16();
        uint16_t hcrc = port_->read_le16();

        Packet pkt(seq, chan, flags, opcode, length, {});

        bool seq_ok =
            (flags & Flags::EVENT) || (flags & Flags::RTX) || (seq == sequence_) || (opcode == Opcode::PROTO_SYNC);
        if (length > max_payload_ || !seq_ok || hcrc != pkt.hdrCrc()) {
            continue;
        }

        if (length > 0) {
            pkt.payload = port_->read_bytes(length);
            uint32_t pcrc = port_->read_le32();
            if (pcrc != pkt.payloadCrc()) {
                continue;
            }
        }

        return pkt;
    }
    throw std::runtime_error("readPacket failed: max retries exceeded");
}

std::vector<uint8_t> ProtocolV2::readResponse() {
    requireOpen();

    std::vector<uint8_t> fragments;
    constexpr int kMaxIterations = 100;

    for (int iter = 0; iter < kMaxIterations; ++iter) {
        auto pkt = readPacket();

        // Handle retransmission
        if ((pkt.flags & Flags::RTX) && (sequence_ != pkt.sequence)) {
            if (pkt.flags & Flags::ACK_REQ) sendPacket({pkt.sequence, pkt.channel, Flags::ACK, pkt.opcode, 0, {}});
            continue;
        }

        // ACK the received packet
        if (pkt.flags & Flags::ACK_REQ) sendPacket({sequence_, pkt.channel, Flags::ACK, pkt.opcode, 0, {}});

        // Handle event packets
        if (pkt.flags & Flags::EVENT) {
            uint16_t evt = 0xFFFF;
            if (pkt.payload.size() >= 2) std::memcpy(&evt, pkt.payload.data(), 2);
            handleEvent(pkt.channel, evt);
            continue;
        }

        // Update sequence
        sequence_ = static_cast<uint8_t>((sequence_ + 1) & 0xFF);

        // Handle fragments
        if (pkt.flags & Flags::FRAGMENT) {
            fragments.insert(fragments.end(), pkt.payload.begin(), pkt.payload.end());
            continue;
        }

        // Last fragment or non-fragmented
        if (!fragments.empty()) {
            fragments.insert(fragments.end(), pkt.payload.begin(), pkt.payload.end());
            pkt.payload = std::move(fragments);
            fragments.clear();
        }

        // Handle NAK
        if (pkt.flags & Flags::NAK) {
            uint16_t status = 0;
            if (pkt.payload.size() >= 2) std::memcpy(&status, pkt.payload.data(), 2);

            if (status == Status::CHECKSUM) throw std::runtime_error("Checksum error from device");
            if (status == Status::SEQUENCE) throw std::runtime_error("Sequence error from device");
            if (status == Status::TIMEOUT) throw std::runtime_error("Timeout from device");
            if (status == Status::BUSY) throw std::runtime_error("Device busy");
            throw std::runtime_error("Command failed with status: " + std::to_string(status));
        }

        return std::move(pkt.payload);
    }
    throw std::runtime_error("readResponse failed: max iterations exceeded");
}

void ProtocolV2::connect(std::shared_ptr<SerialPort> port) {
    disconnect();
    port_ = std::move(port);

    requireOpen();

    try {
        constexpr int kMaxRetries = 3;
        for (int attempt = 0; attempt < kMaxRetries; attempt++) {
            try {
                resync();
                break;
            } catch (const std::runtime_error&) {
                if (attempt < kMaxRetries - 1) continue;
                throw std::runtime_error("Resync failed - unable to synchronize with device");
            }
        }

        sendPacket({sequence_, 0, 0, Opcode::PROTO_VERSION, 0, {}});
        auto ver_data = readResponse();
        if (ver_data.size() >= 16) {
            systemInfo.fw_version[0] = ver_data[6];
            systemInfo.fw_version[1] = ver_data[7];
            systemInfo.fw_version[2] = ver_data[8];
        }

        sendPacket({sequence_, 0, 0, Opcode::SYS_INFO, 0, {}});
        auto info_data = readResponse();
        if (info_data.size() >= 76) {
            const uint8_t* p = info_data.data();
            for (int i = 0; i < 3; i++)
                std::memcpy(&systemInfo.device_id[i], p + 4 + (static_cast<ptrdiff_t>(i) * 4), 4);
            uint32_t usb_id;
            std::memcpy(&usb_id, p + 16, 4);
            systemInfo.vid = static_cast<uint16_t>(usb_id >> 16);
            systemInfo.pid = static_cast<uint16_t>(usb_id & 0xFFFF);
            for (int i = 0; i < 3; i++)
                std::memcpy(&systemInfo.sensor_chip_id[i], p + 20 + (static_cast<ptrdiff_t>(i) * 4), 4);
            std::memcpy(&systemInfo.capabilities, p + 40, 4);
        }

        // Stop any running script
        channelIoctl(stdin_channel_, StdinIoctl::STOP);

        systemInfo.protocol_version = 2;
        startLoopThread();
    } catch (...) {
        disconnect();
        throw;
    }
}

void ProtocolV2::disconnect() {
    Protocol::disconnect();
    sequence_ = 0;
    max_payload_ = 0;
    caps_max_payload_ = 4096;
    stdin_channel_ = 0;
    stdout_channel_ = 0;
    channels_stale_ = false;
    resync_pending_ = false;
}

void ProtocolV2::handleEvent(uint8_t channel_id, uint16_t event) {
    if (channel_id == 0) {
        // System events
        switch (event) {
            case EventType::SOFT_REBOOT:
                resync_pending_ = true;
                sequence_ = 0;
                break;
            case EventType::CHANNEL_REGISTERED:
            case EventType::CHANNEL_UNREGISTERED:
                channels_stale_ = true;
                break;
            default:
                break;
        }
    } else if (channel_id == stdin_channel_) {
        script_running_ = (event == 1);
    }
}

void ProtocolV2::discoverChannels() {
    sendPacket({sequence_, 0, 0, Opcode::CHANNEL_LIST, 0, {}});
    auto data = readResponse();

    // Each channel entry is 16 bytes: [id(1), flags(1), name(14)]
    for (size_t i = 0; i + 16 <= data.size(); i += 16) {
        uint8_t id = data[i];
        std::string name(reinterpret_cast<const char*>(data.data() + i + 2), 14);
        auto nul = name.find('\0');
        if (nul != std::string::npos) name.resize(nul);

        if (name == "stdin") stdin_channel_ = id;
        if (name == "stdout") stdout_channel_ = id;
    }
    if (stdin_channel_ == 0 || stdout_channel_ == 0) {
        throw std::runtime_error("Required channels (stdin/stdout) not found");
    }
}

void ProtocolV2::channelIoctl(uint8_t channel, uint32_t cmd) {
    std::vector<uint8_t> payload(4);
    std::memcpy(payload.data(), &cmd, 4);
    sendPacket({sequence_, channel, 0, Opcode::CHANNEL_IOCTL, static_cast<uint16_t>(payload.size()), payload});
    readResponse();
}

void ProtocolV2::channelWrite(uint8_t channel, const std::vector<uint8_t>& data) {
    size_t offset = 0;
    while (offset < data.size()) {
        size_t chunk = std::min(data.size() - offset, static_cast<size_t>(max_payload_) - 8);
        std::vector<uint8_t> payload(8 + chunk);
        auto off32 = static_cast<uint32_t>(offset);
        auto len32 = static_cast<uint32_t>(chunk);
        std::memcpy(payload.data(), &off32, 4);
        std::memcpy(payload.data() + 4, &len32, 4);
        std::memcpy(payload.data() + 8, data.data() + offset, chunk);
        sendPacket({sequence_, channel, 0, Opcode::CHANNEL_WRITE, static_cast<uint16_t>(payload.size()), payload});
        readResponse();
        offset += chunk;
    }
}

uint32_t ProtocolV2::channelPoll() {
    sendPacket({sequence_, 0, 0, Opcode::CHANNEL_POLL, 0, {}});
    auto data = readResponse();
    if (data.size() < 4) return 0;
    uint32_t flags = 0;
    std::memcpy(&flags, data.data(), 4);
    return flags;
}

uint32_t ProtocolV2::channelSize(uint8_t channel) {
    sendPacket({sequence_, channel, 0, Opcode::CHANNEL_SIZE, 0, {}});
    auto data = readResponse();
    if (data.size() < 4) return 0;
    uint32_t size = 0;
    std::memcpy(&size, data.data(), 4);
    return size;
}

std::vector<uint8_t> ProtocolV2::channelRead(uint8_t channel, uint32_t offset, uint32_t len) {
    std::vector<uint8_t> payload(8);
    std::memcpy(payload.data(), &offset, 4);
    std::memcpy(payload.data() + 4, &len, 4);
    sendPacket({sequence_, channel, 0, Opcode::CHANNEL_READ, static_cast<uint16_t>(payload.size()), payload});
    return readResponse();
}

void ProtocolV2::execScript(const std::string& script) {
    requireOpen();

    std::vector<uint8_t> script_data(script.begin(), script.end());

    std::lock_guard<std::mutex> lock(io_mutex_);
    channelIoctl(stdin_channel_, StdinIoctl::RESET);
    channelWrite(stdin_channel_, script_data);
    channelIoctl(stdin_channel_, StdinIoctl::EXEC);
}

void ProtocolV2::stopScript() {
    requireOpen();

    std::lock_guard<std::mutex> lock(io_mutex_);
    channelIoctl(stdin_channel_, StdinIoctl::STOP);
}

void ProtocolV2::resync() {
    sequence_ = 0;
    max_payload_ = Proto::MIN_PAYLOAD_SIZE;
    sendPacket({sequence_, 0, 0, Opcode::PROTO_SYNC, 0, {}});
    readResponse();
    sequence_ = 0;

    sendPacket({sequence_, 0, 0, Opcode::PROTO_GET_CAPS, 0, {}});
    auto caps_payload = readResponse();
    if (caps_payload.size() >= 6) {
        uint16_t remote_max;
        std::memcpy(&remote_max, caps_payload.data() + 4, 2);
        caps_max_payload_ = std::min(remote_max, caps_max_payload_);
    }

    uint32_t caps_flags = 0x0F;
    std::vector<uint8_t> caps_out(16, 0);
    std::memcpy(caps_out.data(), &caps_flags, 4);
    std::memcpy(caps_out.data() + 4, &caps_max_payload_, 2);
    sendPacket({sequence_, 0, 0, Opcode::PROTO_SET_CAPS, static_cast<uint16_t>(caps_out.size()), caps_out});
    readResponse();

    max_payload_ = caps_max_payload_;
    discoverChannels();
    channels_stale_ = false;
    script_running_ = false;
}

void ProtocolV2::poll() {
    std::lock_guard<std::mutex> lock(io_mutex_);

    if (resync_pending_.exchange(false)) {
        try {
            resync();
        } catch (const std::exception&) {
            resync_pending_ = true;
            return;
        }
    }

    if (channels_stale_) {
        try {
            discoverChannels();
            channels_stale_ = false;
        } catch (const std::exception&) {
            // Will retry on next poll
        }
    }

    uint32_t flags = channelPoll();
    script_running_.store((flags & (1u << stdin_channel_)) != 0);
    if (!(flags & (1u << stdout_channel_))) return;

    uint32_t available = channelSize(stdout_channel_);
    if (available == 0) return;
    auto data = channelRead(stdout_channel_, 0, available);
    terminal_buf_.append(data);
}

}  // namespace mcp
