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
        std::memcpy(&frame_width_, data.data() + 4, 4);
        std::memcpy(&frame_height_, data.data() + 8, 4);
        std::memcpy(&frame_bpp_, data.data() + 12, 4);

        if ((flags_ & kFlagText) != 0) {
            text_.assign(data.begin() + kHeaderSize, data.end());
            while (!text_.empty() && text_.back() == 0) {
                text_.pop_back();
            }
        }
    }

    [[nodiscard]] bool scriptRunning() const { return (flags_ & kFlagScript) != 0; }
    [[nodiscard]] bool hasText() const { return (flags_ & kFlagText) != 0; }
    [[nodiscard]] bool hasFrame() const { return (flags_ & kFlagFrame) != 0; }
    [[nodiscard]] uint32_t frameWidth() const { return frame_width_; }
    [[nodiscard]] uint32_t frameHeight() const { return frame_height_; }
    [[nodiscard]] uint32_t frameBpp() const { return frame_bpp_; }

    [[nodiscard]] const std::vector<uint8_t>& textData() const { return text_; }

 private:
    static constexpr uint32_t kFlagScript = 1 << 0;
    static constexpr uint32_t kFlagText = 1 << 1;
    static constexpr uint32_t kFlagFrame = 1 << 2;

    uint32_t flags_ = 0;
    uint32_t frame_width_ = 0;
    uint32_t frame_height_ = 0;
    uint32_t frame_bpp_ = 0;
    std::vector<uint8_t> text_;
};

class ProtocolV1 : public Protocol {
 public:
    ~ProtocolV1() override { disconnect(); }
    void connect(std::shared_ptr<SerialPort> port) override;
    void execScript(const std::string& script) override;
    void stopScript() override;
    void enableFrame(bool enable) override;

 private:
    void sendCommand(uint8_t opcode, uint32_t len);
    void readFwVersion();
    void readArchStr();
    void readSensorId();
    void poll() override;
    static void delay(int ms);
};

}  // namespace mcp
