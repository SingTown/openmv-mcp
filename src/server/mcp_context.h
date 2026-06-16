#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "camera.h"

namespace mcp {

using json = nlohmann::ordered_json;

inline void writeStreamEvent(std::ostream& os, const json& data) {
    os << "event: message\ndata: " << data.dump() << "\n\n" << std::flush;
}

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

    std::shared_ptr<Camera> getCamera(const std::string& path) {
        std::lock_guard lock(cameras_mutex_);
        auto it = cameras_.find(path);
        if (it == cameras_.end()) {
            throw std::runtime_error("Camera not connected: " + path);
        }
        return it->second;
    }

    [[nodiscard]] bool hasCamera(const std::string& path) const {
        std::lock_guard lock(cameras_mutex_);
        return cameras_.find(path) != cameras_.end();
    }

    std::shared_ptr<Camera> addCamera(const std::string& path, std::unique_ptr<Camera> camera) {
        std::lock_guard lock(cameras_mutex_);
        if (cameras_.count(path) != 0) {
            throw std::runtime_error("Camera already connected: " + path);
        }
        auto shared = std::shared_ptr<Camera>(std::move(camera));
        cameras_[path] = shared;
        return shared;
    }

    void removeCamera(const std::string& path) {
        auto cam = takeCamera(path);
        cam->disconnect();
    }

    std::shared_ptr<Camera> takeCamera(const std::string& path) {
        std::lock_guard lock(cameras_mutex_);
        auto it = cameras_.find(path);
        if (it == cameras_.end()) {
            throw std::runtime_error("Camera not connected: " + path);
        }
        auto cam = std::move(it->second);
        cameras_.erase(it);
        return cam;
    }

 private:
    mutable std::mutex cameras_mutex_;
    std::map<std::string, std::shared_ptr<Camera>> cameras_;
};

}  // namespace mcp
