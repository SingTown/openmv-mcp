#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "frame.h"
#include "serial_port/serial_port.h"
#include "utils/utf8_buffer.h"

namespace mcp {

struct SystemInfo {
    uint32_t device_id[3] = {};
    uint32_t sensor_chip_id[3] = {};
    uint32_t capabilities = 0;
    uint32_t fw_version[3] = {};
    uint16_t vid = 0;
    uint16_t pid = 0;
    uint32_t protocol_version = 0;
    std::string board_type;
    std::string board_name;
    bool licensed = true;

    std::string deviceIdHex() const {
        char buf[25];
        std::snprintf(buf, sizeof(buf), "%08X%08X%08X", device_id[0], device_id[1], device_id[2]);
        return buf;
    }
};

class Camera {
 public:
    using CallbackId = int;
    using ConnectedCallback = std::function<void(bool connected)>;
    using ScriptCallback = std::function<void(bool running)>;
    using TerminalCallback = std::function<void(const std::string& text)>;
    using FrameCallback = std::function<void(const Frame& frame)>;

    virtual ~Camera() = default;

    static std::unique_ptr<Camera> create(const std::string& path);

    virtual void disconnect();

    virtual void reset() = 0;
    virtual void boot() = 0;
    virtual void execScript(const std::string& script) = 0;
    virtual void stopScript() = 0;
    virtual void enableFrame(bool enable) = 0;

    [[nodiscard]] std::string readTerminal();
    [[nodiscard]] std::optional<Frame> readFrame();
    [[nodiscard]] bool isConnected() const { return connected_.load(); }
    [[nodiscard]] bool scriptRunning() const { return script_running_.load(); }

    CallbackId onConnected(ConnectedCallback cb);
    CallbackId onScript(ScriptCallback cb);
    CallbackId onTerminal(TerminalCallback cb);
    CallbackId onFrame(FrameCallback cb);
    void removeCallback(CallbackId id);

    SystemInfo systemInfo;

 protected:
    static constexpr int kPollMs = 10;

    virtual void connect(std::shared_ptr<SerialPort> port) = 0;
    virtual void poll() = 0;

    void startLoopThread();
    void stopLoopThread();

    void requireOpen() const {
        if (!port_ || !port_->isOpen()) throw std::runtime_error("Serial connection not open");
    }

    void updateConnected(bool connected);
    void updateScript(bool running);
    void appendTerminal(const std::vector<uint8_t>& data);
    void setFrame(Frame frame);

    std::shared_ptr<SerialPort> port_;
    std::thread loop_thread_;
    std::mutex io_mutex_;
    Utf8Buffer terminal_buf_;
    std::mutex frame_mutex_;
    std::optional<Frame> frame_;
    std::atomic<bool> loop_running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> script_running_{false};

 private:
    template <typename Cb>
    CallbackId addCallback(std::map<CallbackId, Cb>& map, Cb cb);

    std::mutex callback_mutex_;
    CallbackId next_cb_id_{0};
    std::map<CallbackId, ConnectedCallback> on_connected_cbs_;
    std::map<CallbackId, ScriptCallback> on_script_cbs_;
    std::map<CallbackId, TerminalCallback> on_terminal_cbs_;
    Utf8Buffer terminal_cb_buf_;
    std::map<CallbackId, FrameCallback> on_frame_cbs_;
};

}  // namespace mcp
