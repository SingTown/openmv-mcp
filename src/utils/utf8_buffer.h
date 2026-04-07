#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace mcp {

/// Streaming UTF-8 buffer: appends raw bytes, returns only complete
/// UTF-8 on take(), keeping incomplete trailing bytes for next append.
class Utf8Buffer {
    static constexpr size_t kDefaultMaxSize = size_t{1024} * 1024;  // 1MB

 public:
    explicit Utf8Buffer(size_t max_size = kDefaultMaxSize) : max_size_(max_size) {}

    void append(const std::string& data) { append(data.data(), data.size()); }

    void append(const char* data, size_t len) {
        std::lock_guard<std::mutex> lock(mutex_);
        buf_.append(data, len);
        trim();
    }

    void append(const std::vector<uint8_t>& data) { append(reinterpret_cast<const char*>(data.data()), data.size()); }

    std::string take() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buf_.empty()) {
            return {};
        }
        size_t valid = findValidEnd(buf_.data(), buf_.size());
        if (valid == 0) {
            return {};
        }
        if (valid == buf_.size()) {
            return std::exchange(buf_, {});
        }
        std::string result = buf_.substr(0, valid);
        buf_.erase(0, valid);
        return result;
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buf_.size();
    }
    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buf_.empty();
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buf_.clear();
    }

 private:
    static size_t findValidEnd(const char* data, size_t len) {
        if (len == 0) {
            return 0;
        }
        // Walk backwards past continuation bytes (10xxxxxx) to find the last lead byte
        size_t i = len;
        while (i > 0 && (static_cast<uint8_t>(data[i - 1]) & 0xC0) == 0x80) {
            --i;
        }
        if (i == 0) {
            return 0;  // all continuation bytes
        }
        size_t lead = i - 1;
        size_t expected = charLen(static_cast<uint8_t>(data[lead]));
        size_t actual = len - lead;
        return (actual >= expected) ? len : lead;
    }

    void trim() {
        if (buf_.size() > max_size_) {
            size_t drop = buf_.size() - max_size_;
            while (drop < buf_.size() && (static_cast<uint8_t>(buf_[drop]) & 0xC0) == 0x80) {
                ++drop;
            }
            buf_.erase(0, drop);
        }
    }

    static size_t charLen(uint8_t lead) {
        if (lead < 0x80) {
            return 1;
        }
        if ((lead & 0xE0) == 0xC0) {
            return 2;
        }
        if ((lead & 0xF0) == 0xE0) {
            return 3;
        }
        if ((lead & 0xF8) == 0xF0) {
            return 4;
        }
        return 1;  // invalid lead byte, treat as single byte
    }

    mutable std::mutex mutex_;
    size_t max_size_;
    std::string buf_;
};

}  // namespace mcp
