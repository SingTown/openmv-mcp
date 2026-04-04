#include "utils/utf8_buffer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace mcp {

static std::vector<uint8_t> bytes(const std::string& s) {
    return {s.begin(), s.end()};
}

static std::vector<uint8_t> raw(std::initializer_list<uint8_t> il) {
    return il;
}

TEST(Utf8BufferTest, AppendAndTake) {
    Utf8Buffer buf;
    buf.append(bytes("hello"));
    EXPECT_EQ(buf.take(), "hello");
    EXPECT_TRUE(buf.empty());
}

TEST(Utf8BufferTest, IncompleteTrailingBytesRetained) {
    Utf8Buffer buf;
    // "你" = E4 BD A0, append only first 2 bytes
    buf.append(raw({0x41, 0xE4, 0xBD}));
    EXPECT_EQ(buf.take(), "A");
    // Complete the character
    buf.append(raw({0xA0}));
    EXPECT_EQ(buf.take(), "你");
}

TEST(Utf8BufferTest, TruncationSkipsOrphanedContinuationBytes) {
    // max_size=4, buf="A\xE4\xBD\xA0\xE5\xA5\xBD" (7 bytes)
    // erase(0,3) → "\xA0\xE5\xA5\xBD", skip \xA0 → "好"
    Utf8Buffer buf(4);
    buf.append(bytes("A\xe4\xbd\xa0\xe5\xa5\xbd"));
    EXPECT_EQ(buf.take(), "好");
}

TEST(Utf8BufferTest, EmptyTakeReturnsEmpty) {
    Utf8Buffer buf;
    EXPECT_EQ(buf.take(), "");
    EXPECT_TRUE(buf.empty());
}

TEST(Utf8BufferTest, PureContinuationBytesReturnEmpty) {
    Utf8Buffer buf;
    buf.append(raw({0x80, 0x80}));
    EXPECT_EQ(buf.take(), "");
    EXPECT_EQ(buf.size(), 2);
}

TEST(Utf8BufferTest, FourByteEmojiSplitAndReassemble) {
    Utf8Buffer buf;
    // 😀 = F0 9F 98 80, split after 2 bytes
    buf.append(raw({0xF0, 0x9F}));
    EXPECT_EQ(buf.take(), "");
    buf.append(raw({0x98, 0x80}));
    EXPECT_EQ(buf.take(), "\xF0\x9F\x98\x80");
    EXPECT_TRUE(buf.empty());
}

TEST(Utf8BufferTest, ConcurrentAppendAndTake) {
    Utf8Buffer buf;
    constexpr int kIterations = 10000;
    std::atomic<size_t> taken_bytes{0};

    std::thread writer([&] {
        for (int i = 0; i < kIterations; ++i) {
            buf.append(bytes("AB"));
        }
    });

    std::thread reader([&] {
        for (int i = 0; i < kIterations; ++i) {
            taken_bytes += buf.take().size();
        }
    });

    writer.join();
    reader.join();

    // Drain remaining
    taken_bytes += buf.take().size();
    EXPECT_EQ(taken_bytes.load(), kIterations * 2);
}

}  // namespace mcp
