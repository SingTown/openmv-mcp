#ifdef _WIN32

#include "serial_port.h"

namespace mcp {

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& /*path*/) {
    return false;
}

void SerialPort::close() {}

std::vector<uint8_t> SerialPort::read(int /*timeoutMs*/) const {
    return {};
}

bool SerialPort::write(const std::vector<uint8_t>& /*data*/) {
    return false;
}

}  // namespace mcp

#endif  // _WIN32
