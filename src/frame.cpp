#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "frame.h"

#include <cstring>
#include <stdexcept>

#include "stb/stb_image_write.h"
#include "utils/base64.h"

namespace mcp {

namespace {

void stbWriteCallback(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

std::vector<uint8_t> rgb565ToRgb888(const uint8_t* src, uint32_t width, uint32_t height) {
    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
    size_t pixels = static_cast<size_t>(width) * height;
    for (size_t i = 0; i < pixels; i++) {
        uint16_t pixel = 0;
        std::memcpy(&pixel, src + (i * 2), 2);
        rgb[(i * 3) + 0] = static_cast<uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
        rgb[(i * 3) + 1] = static_cast<uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
        rgb[(i * 3) + 2] = static_cast<uint8_t>((pixel & 0x1F) * 255 / 31);
    }
    return rgb;
}

std::vector<uint8_t> argb8ToRgb888(const uint8_t* src, uint32_t width, uint32_t height) {
    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
    size_t pixels = static_cast<size_t>(width) * height;
    for (size_t i = 0; i < pixels; i++) {
        rgb[(i * 3) + 0] = src[(i * 4) + 1];  // R
        rgb[(i * 3) + 1] = src[(i * 4) + 2];  // G
        rgb[(i * 3) + 2] = src[(i * 4) + 3];  // B
    }
    return rgb;
}

std::vector<uint8_t> binaryToGray(const uint8_t* src, uint32_t width, uint32_t height) {
    size_t pixels = static_cast<size_t>(width) * height;
    std::vector<uint8_t> gray(pixels);
    for (size_t i = 0; i < pixels; i++) {
        gray[i] = (((src[i / 8] >> (i % 8)) & 1) != 0) ? 0xFF : 0x00;
    }
    return gray;
}

}  // namespace

std::vector<uint8_t> Frame::toJpeg(int quality) const {
    if (pixformat_ == PixFormat::JPEG) {
        return data_;
    }
    if (pixformat_ == PixFormat::PNG) {
        throw std::runtime_error("Cannot convert PNG to JPEG");
    }

    std::vector<uint8_t> out;
    auto w = static_cast<int>(width_);
    auto h = static_cast<int>(height_);

    if (pixformat_ == PixFormat::GRAYSCALE) {
        stbi_write_jpg_to_func(stbWriteCallback, &out, w, h, 1, data_.data(), quality);
    } else if (pixformat_ == PixFormat::RGB565) {
        auto rgb = rgb565ToRgb888(data_.data(), width_, height_);
        stbi_write_jpg_to_func(stbWriteCallback, &out, w, h, 3, rgb.data(), quality);
    } else if (pixformat_ == PixFormat::ARGB8) {
        auto rgb = argb8ToRgb888(data_.data(), width_, height_);
        stbi_write_jpg_to_func(stbWriteCallback, &out, w, h, 3, rgb.data(), quality);
    } else if (pixformat_ == PixFormat::BINARY) {
        auto gray = binaryToGray(data_.data(), width_, height_);
        stbi_write_jpg_to_func(stbWriteCallback, &out, w, h, 1, gray.data(), quality);
    } else {
        throw std::runtime_error("Unsupported pixel format");
    }

    if (out.empty()) {
        throw std::runtime_error("JPEG encoding failed");
    }
    return out;
}

std::string Frame::toBase64Jpeg(int quality) const {
    auto jpeg = toJpeg(quality);
    return base64Encode(jpeg);
}

}  // namespace mcp
