#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mcp {

class SerialPort {
 public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open(const std::string& path);
    void close();

    std::vector<uint8_t> read(int timeoutMs = 1000) const;
    bool write(const std::vector<uint8_t>& data);

 private:
#ifdef _WIN32
    void* handle_ = reinterpret_cast<void*>(-1);  // INVALID_HANDLE_VALUE
#else
    int fd_ = -1;
#endif
};

}  // namespace mcp
