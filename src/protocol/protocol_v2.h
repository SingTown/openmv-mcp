#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "protocol.h"
#include "utils/crc.h"

namespace mcp {

class Packet {
 public:
    Packet(uint8_t seq, uint8_t chan, uint8_t fl, uint8_t op, uint16_t len, std::vector<uint8_t> data)
        : sequence(seq), channel(chan), flags(fl), opcode(op), length(len), payload(std::move(data)) {}

    uint8_t sequence;
    uint8_t channel;
    uint8_t flags;
    uint8_t opcode;
    uint16_t length;
    std::vector<uint8_t> payload;

    [[nodiscard]] uint16_t hdrCrc() const {
        uint8_t raw[8] = {0xAA, 0xD5, sequence, channel, flags, opcode, 0, 0};
        std::memcpy(raw + 6, &length, 2);
        return crc16(raw, 8);
    }

    [[nodiscard]] uint32_t payloadCrc() const { return crc32(payload.data(), payload.size()); }
};

class ProtocolV2 : public Protocol {
 public:
    void connect(std::shared_ptr<SerialPort> port) override;
    void disconnect() override;

 private:
    uint8_t sequence_ = 0;
    uint16_t max_payload_ = 0;
    uint16_t caps_max_payload_ = 4096;
    void sendPacket(const Packet& pkt);
    Packet readPacket();
    std::vector<uint8_t> readResponse();
    void handleEvent(uint8_t channel_id, uint16_t event);
};

}  // namespace mcp
