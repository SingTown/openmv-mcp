#include "serial_port.h"

#include <chrono>
#include <stdexcept>

namespace mcp {

void SerialPort::write_u8(uint8_t v) {
    write_buf_.push_back(v);
}

void SerialPort::write_le16(uint16_t v) {
    uint8_t tmp[2];
    std::memcpy(tmp, &v, 2);
    write_buf_.insert(write_buf_.end(), tmp, tmp + 2);
}

void SerialPort::write_le32(uint32_t v) {
    uint8_t tmp[4];
    std::memcpy(tmp, &v, 4);
    write_buf_.insert(write_buf_.end(), tmp, tmp + 4);
}

void SerialPort::write_bytes(const std::vector<uint8_t>& data) {
    write_buf_.insert(write_buf_.end(), data.begin(), data.end());
}

void SerialPort::waitForData(size_t n) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kTimeoutMs);
    while (recv_buf_.size() < n) {
        if (std::chrono::steady_clock::now() >= deadline) {
            throw std::runtime_error("Read timeout");
        }
        recv();
    }
}

std::vector<uint8_t> SerialPort::read_bytes(size_t n) {
    waitForData(n);
    std::vector<uint8_t> result(n);
    recv_buf_.pop_front(result.data(), n);
    return result;
}

uint8_t SerialPort::read_u8() {
    waitForData(1);
    return recv_buf_.pop_front();
}

uint16_t SerialPort::read_le16() {
    waitForData(2);
    uint8_t tmp[2];
    recv_buf_.pop_front(tmp, 2);
    uint16_t v;
    std::memcpy(&v, tmp, 2);
    return v;
}

uint32_t SerialPort::read_le32() {
    waitForData(4);
    uint8_t tmp[4];
    recv_buf_.pop_front(tmp, 4);
    uint32_t v;
    std::memcpy(&v, tmp, 4);
    return v;
}

}  // namespace mcp
