#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mcp {

namespace PixFormat {
constexpr uint32_t BINARY = 0x08010000;
constexpr uint32_t GRAYSCALE = 0x08020001;
constexpr uint32_t RGB565 = 0x0C030002;
constexpr uint32_t ARGB8 = 0x0C080004;
constexpr uint32_t JPEG = 0x06060000;
constexpr uint32_t PNG = 0x06070000;
}  // namespace PixFormat

class Frame {
 public:
    Frame() = default;
    Frame(uint32_t w, uint32_t h, uint32_t fmt, std::vector<uint8_t> pixels)
        : width_(w), height_(h), pixformat_(fmt), data_(std::move(pixels)) {}

    [[nodiscard]] std::vector<uint8_t> toJpeg(int quality = 80) const;
    [[nodiscard]] std::string toBase64Jpeg(int quality = 80) const;

 private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t pixformat_ = 0;
    std::vector<uint8_t> data_;
};

}  // namespace mcp
