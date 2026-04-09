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
    updateScript(false);
    updateConnected(false);
}

std::string Camera::readTerminal() {
    return terminal_buf_.take();
}

std::optional<Frame> Camera::readFrame() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return std::exchange(frame_, std::nullopt);
}

template <typename Cb>
Camera::CallbackId Camera::addCallback(std::map<CallbackId, Cb>& map, Cb cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    auto id = next_cb_id_++;
    map.emplace(id, std::move(cb));
    return id;
}

Camera::CallbackId Camera::onConnected(ConnectedCallback cb) {
    return addCallback(on_connected_cbs_, std::move(cb));
}
Camera::CallbackId Camera::onScript(ScriptCallback cb) {
    return addCallback(on_script_cbs_, std::move(cb));
}
Camera::CallbackId Camera::onTerminal(TerminalCallback cb) {
    return addCallback(on_terminal_cbs_, std::move(cb));
}
Camera::CallbackId Camera::onFrame(FrameCallback cb) {
    return addCallback(on_frame_cbs_, std::move(cb));
}

void Camera::removeCallback(CallbackId id) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_connected_cbs_.erase(id);
    on_script_cbs_.erase(id);
    on_terminal_cbs_.erase(id);
    on_frame_cbs_.erase(id);
}

void Camera::updateConnected(bool connected) {
    bool prev = connected_.exchange(connected);
    if (prev != connected) {
        std::vector<ConnectedCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            cbs.reserve(on_connected_cbs_.size());
            for (const auto& [id, cb] : on_connected_cbs_) {
                cbs.push_back(cb);
            }
        }
        for (const auto& cb : cbs) {
            cb(connected);
        }
    }
}

void Camera::updateScript(bool running) {
    bool prev = script_running_.exchange(running);
    if (prev != running) {
        std::vector<ScriptCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            cbs.reserve(on_script_cbs_.size());
            for (const auto& [id, cb] : on_script_cbs_) {
                cbs.push_back(cb);
            }
        }
        for (const auto& cb : cbs) {
            cb(running);
        }
    }
}

void Camera::appendTerminal(const std::vector<uint8_t>& data) {
    terminal_buf_.append(data);
    std::vector<TerminalCallback> cbs;
    std::string text;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (on_terminal_cbs_.empty()) return;
        terminal_cb_buf_.append(data);
        text = terminal_cb_buf_.take();
        if (text.empty()) return;
        cbs.reserve(on_terminal_cbs_.size());
        for (const auto& [id, cb] : on_terminal_cbs_) {
            cbs.push_back(cb);
        }
    }
    for (const auto& cb : cbs) {
        cb(text);
    }
}

void Camera::setFrame(Frame frame) {
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        frame_ = frame;
    }
    std::vector<FrameCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cbs.reserve(on_frame_cbs_.size());
        for (const auto& [id, cb] : on_frame_cbs_) {
            cbs.push_back(cb);
        }
    }
    for (const auto& cb : cbs) {
        cb(frame);
    }
}

void Camera::startLoopThread() {
    if (loop_thread_.joinable()) return;
    loop_running_ = true;
    updateConnected(true);
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
                    port_->close();
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        }
        updateConnected(false);
    });
}

void Camera::stopLoopThread() {
    loop_running_ = false;
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

}  // namespace mcp
