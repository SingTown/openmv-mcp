#include "ring_buffer.h"

#include <gtest/gtest.h>

namespace mcp {

TEST(RingBufferTest, PushAndPop) {
    RingBuffer<uint8_t> rb(16);
    uint8_t data[] = {1, 2, 3};
    rb.push_back(data, 3);
    EXPECT_EQ(rb.size(), 3u);
    EXPECT_EQ(rb.pop_front(), 1);
    EXPECT_EQ(rb.pop_front(), 2);
    EXPECT_EQ(rb.pop_front(), 3);
    EXPECT_TRUE(rb.empty());
}

TEST(RingBufferTest, Wraparound) {
    RingBuffer<uint8_t> rb(4);
    uint8_t a[] = {1, 2, 3};
    rb.push_back(a, 3);
    rb.pop_front();
    rb.pop_front();

    uint8_t b[] = {4, 5, 6};
    rb.push_back(b, 3);

    uint8_t out[4];
    rb.pop_front(out, 4);
    EXPECT_EQ(out[0], 3);
    EXPECT_EQ(out[1], 4);
    EXPECT_EQ(out[2], 5);
    EXPECT_EQ(out[3], 6);
}

TEST(RingBufferTest, OverflowThrows) {
    RingBuffer<uint8_t> rb(2);
    uint8_t data[] = {1, 2};
    rb.push_back(data, 2);
    uint8_t extra[] = {3};
    EXPECT_THROW(rb.push_back(extra, 1), std::overflow_error);
}

TEST(RingBufferTest, UnderflowThrows) {
    RingBuffer<uint8_t> rb(4);
    EXPECT_THROW(rb.pop_front(), std::underflow_error);
}

}  // namespace mcp
