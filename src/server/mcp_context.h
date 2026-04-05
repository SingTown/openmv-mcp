#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "camera.h"

namespace mcp {

class McpContext {
 public:
    McpContext() = default;
    ~McpContext() {
        for (auto& [path, cam] : cameras_) {
            try {
                cam->disconnect();
            } catch (...) {
            }
        }
    }
    McpContext(const McpContext&) = delete;
    McpContext& operator=(const McpContext&) = delete;

    Camera& getCamera(const std::string& path) {
        auto it = cameras_.find(path);
        if (it == cameras_.end()) {
            throw std::runtime_error("Camera not connected: " + path);
        }
        return *it->second;
    }

    void addCamera(const std::string& path, std::unique_ptr<Camera> camera) {
        if (cameras_.count(path) != 0) {
            throw std::runtime_error("Camera already connected: " + path);
        }
        cameras_[path] = std::move(camera);
    }

    void removeCamera(const std::string& path) {
        auto it = cameras_.find(path);
        if (it == cameras_.end()) {
            throw std::runtime_error("Camera not connected: " + path);
        }
        it->second->disconnect();
        cameras_.erase(it);
    }

 private:
    std::map<std::string, std::unique_ptr<Camera>> cameras_;
};

}  // namespace mcp
