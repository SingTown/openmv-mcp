#include "camera.h"

#include <iostream>
#include <memory>

#include "protocol/protocol_detect.h"
#include "protocol/protocol_v1.h"
#include "protocol/protocol_v2.h"

namespace mcp {

std::unique_ptr<Camera> Camera::create(const std::string& path) {
    auto port = std::make_shared<SerialPort>();
    if (!port->open(path)) {
        throw std::runtime_error("Failed to open serial port: " + path);
    }

    int protoVersion = detectProtocol(*port);
    std::unique_ptr<Camera> camera;
    if (protoVersion == 1) {
        camera = std::make_unique<ProtocolV1>();
    } else {
        camera = std::make_unique<ProtocolV2>();
    }
    try {
        camera->connect(port);
    } catch (...) {
        port->close();
        throw;
    }
    return camera;
}

void Camera::disconnect() {
    stopLoopThread();
    port_.reset();
    systemInfo = {};
    script_running_ = false;
}

std::string Camera::readTerminal() {
    return terminal_buf_.take();
}

std::optional<Frame> Camera::readFrame() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return std::exchange(frame_, std::nullopt);
}

void Camera::startLoopThread() {
    if (loop_thread_.joinable()) return;
    loop_running_ = true;
    loop_thread_ = std::thread([this]() {
        int consecutive_errors = 0;
        constexpr int kMaxConsecutiveErrors = 3;
        while (loop_running_) {
            if (!port_ || !port_->isOpen()) {
                break;
            }
            try {
                poll();
                consecutive_errors = 0;
            } catch (const std::exception& e) {
                std::cerr << "Protocol poll error: " << e.what() << '\n';
                if (++consecutive_errors >= kMaxConsecutiveErrors) {
                    std::cerr << "Protocol loop: " << kMaxConsecutiveErrors << " consecutive errors, stopping.\n";
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        }
        if (port_) {
            port_->close();
        }
    });
}

void Camera::stopLoopThread() {
    loop_running_ = false;
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

}  // namespace mcp
