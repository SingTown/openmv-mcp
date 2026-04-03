#pragma once

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace mcp {

template <typename T>
class RingBuffer {
 public:
    explicit RingBuffer(size_t capacity) : buf_(capacity), capacity_(capacity) {}

    void push_back(const T* data, size_t n) {
        if (count_ + n > capacity_) throw std::overflow_error("RingBuffer overflow");
        size_t first = std::min(n, capacity_ - tail_);
        std::memcpy(buf_.data() + tail_, data, first * sizeof(T));
        if (first < n) std::memcpy(buf_.data(), data + first, (n - first) * sizeof(T));
        tail_ = (tail_ + n) % capacity_;
        count_ += n;
    }

    T pop_front() {
        if (count_ == 0) throw std::underflow_error("RingBuffer underflow");
        T v = buf_[head_];
        head_ = (head_ + 1) % capacity_;
        --count_;
        return v;
    }

    void pop_front(T* out, size_t n) {
        peek(out, n);
        head_ = (head_ + n) % capacity_;
        count_ -= n;
    }

    void peek(T* out, size_t n) const {
        if (n > count_) throw std::underflow_error("RingBuffer underflow");
        size_t first = std::min(n, capacity_ - head_);
        std::memcpy(out, buf_.data() + head_, first * sizeof(T));
        if (first < n) std::memcpy(out + first, buf_.data(), (n - first) * sizeof(T));
    }

    [[nodiscard]] size_t size() const { return count_; }
    [[nodiscard]] bool empty() const { return count_ == 0; }

    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

 private:
    std::vector<T> buf_;
    size_t capacity_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

}  // namespace mcp
