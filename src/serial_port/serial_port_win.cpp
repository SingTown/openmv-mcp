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

bool SerialPort::isOpen() const {
    return handle_ != reinterpret_cast<void*>(-1);
}

bool SerialPort::send() {
    write_buf_.clear();
    return false;
}

bool SerialPort::recv() {
    return false;
}

}  // namespace mcp

#endif  // _WIN32
