#include "crc.h"

#include <gtest/gtest.h>

namespace mcp {

TEST(Crc16Test, EmptyData) {
    EXPECT_EQ(crc16(nullptr, 0), 0xFFFF);
}

TEST(Crc16Test, DifferentDataProducesDifferentCrc) {
    uint8_t a[] = {0x01, 0x02, 0x03};
    uint8_t b[] = {0x01, 0x02, 0x04};
    EXPECT_NE(crc16(a, 3), crc16(b, 3));
}

TEST(Crc16Test, IncrementalCrc) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint16_t full = crc16(data, 4);
    uint16_t incremental = crc16(data + 2, 2, crc16(data, 2));
    EXPECT_EQ(full, incremental);
}

TEST(Crc32Test, EmptyData) {
    EXPECT_EQ(crc32(nullptr, 0), 0xFFFFFFFF);
}

TEST(Crc32Test, DifferentDataProducesDifferentCrc) {
    uint8_t a[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t b[] = {0x01, 0x02, 0x03, 0x05};
    EXPECT_NE(crc32(a, 4), crc32(b, 4));
}

TEST(Crc32Test, IncrementalCrc) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint32_t full = crc32(data, 4);
    uint32_t incremental = crc32(data + 2, 2, crc32(data, 2));
    EXPECT_EQ(full, incremental);
}

}  // namespace mcp
