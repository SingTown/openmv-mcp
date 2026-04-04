#pragma once

#include <memory>
#include <string>

#include "nlohmann/json.hpp"

namespace mcp {

class SerialPort;
class Protocol;

class Camera {
 public:
    Camera();
    ~Camera();

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    void connect(const std::string& path);
    void disconnect();
    [[nodiscard]] bool isConnected() const;

    [[nodiscard]] nlohmann::json systemInfo() const;

    void execScript(const std::string& script);
    void stopScript();
    [[nodiscard]] std::string readTerminal();
    [[nodiscard]] bool scriptRunning() const;

 private:
    std::shared_ptr<SerialPort> port_;
    std::unique_ptr<Protocol> protocol_;

    static int detectProtocol(SerialPort& port);
};

}  // namespace mcp
