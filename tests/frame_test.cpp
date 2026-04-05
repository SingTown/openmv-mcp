#include "frame.h"

#include <gtest/gtest.h>

using namespace mcp;

TEST(FrameTest, JpegPassthrough) {
    // JPEG magic bytes
    std::vector<uint8_t> jpeg_data = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10};
    Frame frame(320, 240, PixFormat::JPEG, jpeg_data);

    auto result = frame.toJpeg();
    EXPECT_EQ(result, jpeg_data);
}

TEST(FrameTest, PngThrows) {
    std::vector<uint8_t> png_data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A};
    Frame frame(320, 240, PixFormat::PNG, png_data);

    EXPECT_THROW(frame.toJpeg(), std::runtime_error);
}

TEST(FrameTest, GrayscaleToJpeg) {
    constexpr uint32_t w = 8;
    constexpr uint32_t h = 8;
    std::vector<uint8_t> gray(w * h, 0x80);
    Frame frame(w, h, PixFormat::GRAYSCALE, gray);

    auto result = frame.toJpeg();
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0], 0xFF);
    EXPECT_EQ(result[1], 0xD8);
}

TEST(FrameTest, Rgb565ToJpeg) {
    constexpr uint32_t w = 4;
    constexpr uint32_t h = 4;
    // Red in RGB565: 0xF800 → little-endian {0x00, 0xF8}
    std::vector<uint8_t> rgb565;
    for (uint32_t i = 0; i < w * h; i++) {
        rgb565.push_back(0x00);
        rgb565.push_back(0xF8);
    }
    Frame frame(w, h, PixFormat::RGB565, rgb565);

    auto result = frame.toJpeg();
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0], 0xFF);
    EXPECT_EQ(result[1], 0xD8);
}

TEST(FrameTest, BinaryToJpeg) {
    constexpr uint32_t w = 8;
    constexpr uint32_t h = 2;
    // 8 pixels per byte, 2 rows = 2 bytes
    std::vector<uint8_t> binary = {0xFF, 0xAA};
    Frame frame(w, h, PixFormat::BINARY, binary);

    auto result = frame.toJpeg();
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0], 0xFF);
    EXPECT_EQ(result[1], 0xD8);
}

TEST(FrameTest, Argb8ToJpeg) {
    constexpr uint32_t w = 4;
    constexpr uint32_t h = 4;
    // ARGB8: [A, R, G, B] per pixel
    std::vector<uint8_t> argb;
    for (uint32_t i = 0; i < w * h; i++) {
        argb.push_back(0xFF);  // A
        argb.push_back(0xFF);  // R
        argb.push_back(0x00);  // G
        argb.push_back(0x00);  // B
    }
    Frame frame(w, h, PixFormat::ARGB8, argb);

    auto result = frame.toJpeg();
    ASSERT_GE(result.size(), 2U);
    EXPECT_EQ(result[0], 0xFF);
    EXPECT_EQ(result[1], 0xD8);
}

TEST(FrameTest, ToBase64Jpeg) {
    std::vector<uint8_t> jpeg_data = {0xFF, 0xD8, 0xFF, 0xE0};
    Frame frame(1, 1, PixFormat::JPEG, jpeg_data);

    auto b64 = frame.toBase64Jpeg();
    EXPECT_FALSE(b64.empty());
    EXPECT_EQ(b64, "/9j/4A==");
}
