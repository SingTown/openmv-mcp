#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "utils/ring_buffer.h"

namespace mcp {

class SerialPort {
 public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open(const std::string& path);
    void close();

    [[nodiscard]] bool isOpen() const;

    void write_u8(uint8_t v);
    void write_le16(uint16_t v);
    void write_le32(uint32_t v);
    void write_bytes(const std::vector<uint8_t>& data);
    bool send();

    std::vector<uint8_t> read_bytes(size_t n);
    uint8_t read_u8();
    uint16_t read_le16();
    uint32_t read_le32();

 private:
    static constexpr int kTimeoutMs = 1000;
    void waitForData(size_t n);
    bool recv();

#ifdef _WIN32
    void* handle_ = reinterpret_cast<void*>(-1);  // INVALID_HANDLE_VALUE
#else
    int fd_ = -1;
#endif
    std::vector<uint8_t> write_buf_;
    static constexpr size_t kRecvBufSize = 8192;
    RingBuffer<uint8_t> recv_buf_{kRecvBufSize};
};

}  // namespace mcp
