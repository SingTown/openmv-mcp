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

    bool connect(const std::string& path);
    void disconnect();
    [[nodiscard]] bool isConnected() const;

    [[nodiscard]] nlohmann::json systemInfo() const;

 private:
    std::shared_ptr<SerialPort> port_;
    std::unique_ptr<Protocol> protocol_;

    static int detectProtocol(SerialPort& port);
};

}  // namespace mcp
