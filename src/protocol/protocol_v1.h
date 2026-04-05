#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "protocol.h"

namespace mcp {

class V1State {
 public:
    static constexpr size_t kPayloadLen = 64;
    static constexpr size_t kHeaderSize = 16;

    explicit V1State(const std::vector<uint8_t>& data) {
        if (data.size() < kPayloadLen) {
            throw std::runtime_error("V1State: payload too short");
        }
        std::memcpy(&flags_, data.data(), 4);

        if (flags_ & kFlagText) {
            text_.assign(data.begin() + kHeaderSize, data.end());
            while (!text_.empty() && text_.back() == 0) text_.pop_back();
        }
    }

    [[nodiscard]] bool scriptRunning() const { return (flags_ & kFlagScript) != 0; }
    [[nodiscard]] bool hasText() const { return (flags_ & kFlagText) != 0; }

    [[nodiscard]] const std::vector<uint8_t>& textData() const { return text_; }

 private:
    static constexpr uint32_t kFlagScript = 1 << 0;
    static constexpr uint32_t kFlagText = 1 << 1;

    uint32_t flags_ = 0;
    std::vector<uint8_t> text_;
};

class ProtocolV1 : public Protocol {
 public:
    ~ProtocolV1() override { disconnect(); }
    void connect(std::shared_ptr<SerialPort> port) override;
    void execScript(const std::string& script) override;
    void stopScript() override;

 private:
    void sendCommand(uint8_t opcode, uint32_t len);
    void readFwVersion();
    void readArchStr();
    void readSensorId();
    void poll() override;
    static void delay(int ms);
};

}  // namespace mcp
