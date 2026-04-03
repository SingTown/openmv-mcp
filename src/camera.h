#pragma once

#include <memory>
#include <string>

namespace mcp {

class SerialPort;

class Camera {
 public:
    Camera();
    ~Camera();

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    bool connect(const std::string& path);
    void disconnect();

 private:
    std::unique_ptr<SerialPort> port_;
};

}  // namespace mcp
