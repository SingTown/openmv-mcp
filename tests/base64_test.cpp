#include "utils/base64.h"

#include <gtest/gtest.h>

using namespace mcp;

TEST(Base64Test, Empty) {
    EXPECT_EQ(base64Encode(std::vector<uint8_t>{}), "");
}

TEST(Base64Test, OneByte) {
    // 'A' = 0x41 → padding ==
    EXPECT_EQ(base64Encode(std::vector<uint8_t>{0x41}), "QQ==");
}

TEST(Base64Test, TwoBytes) {
    // 'AB' → padding =
    EXPECT_EQ(base64Encode(std::vector<uint8_t>{0x41, 0x42}), "QUI=");
}

TEST(Base64Test, ThreeBytes) {
    // 'ABC' → no padding
    EXPECT_EQ(base64Encode(std::vector<uint8_t>{0x41, 0x42, 0x43}), "QUJD");
}

TEST(Base64Test, KnownString) {
    // "Hello" → "SGVsbG8="
    std::string s = "Hello";
    std::vector<uint8_t> v(s.begin(), s.end());
    EXPECT_EQ(base64Encode(v), "SGVsbG8=");
}

TEST(Base64Test, BinaryData) {
    // All zero bytes
    std::vector<uint8_t> zeros(6, 0x00);
    EXPECT_EQ(base64Encode(zeros), "AAAAAAAA");
}

TEST(Base64Test, PointerOverload) {
    std::vector<uint8_t> data = {0xFF, 0xD8, 0xFF, 0xE0};
    EXPECT_EQ(base64Encode(data.data(), data.size()), "/9j/4A==");
}
