#include "protocol/protocol.h"

#include <iostream>

namespace mcp {

void Protocol::disconnect() {
    stopLoopThread();
    port_.reset();
    systemInfo = {};
    script_running_ = false;
}

std::string Protocol::readTerminal() {
    return terminal_buf_.take();
}

std::optional<Frame> Protocol::readFrame() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return std::exchange(frame_, std::nullopt);
}

void Protocol::startLoopThread() {
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

void Protocol::stopLoopThread() {
    loop_running_ = false;
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

}  // namespace mcp
