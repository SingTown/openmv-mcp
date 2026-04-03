#include "camera.h"

#include "serial_port/serial_port.h"

namespace mcp {

Camera::Camera() : port_(std::make_unique<SerialPort>()) {}

Camera::~Camera() {
    disconnect();
}

bool Camera::connect(const std::string& path) {
    return port_->open(path);
}

void Camera::disconnect() {
    port_->close();
}

}  // namespace mcp
