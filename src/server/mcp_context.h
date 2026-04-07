#pragma once

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <ostream>
#include <stdexcept>
#include <string>

#include "camera.h"

namespace mcp {

using json = nlohmann::json;

inline void writeStreamEvent(std::ostream& os, const json& data) {
    os << "event: message\ndata: " << data.dump() << "\n\n" << std::flush;
}

class McpContext {
 public:
    std::ostream* stream = nullptr;

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

    void streamMessage(const std::string& message, const std::string& level = "info") {
        if (stream != nullptr) {
            writeStreamEvent(*stream,
                             json({{"jsonrpc", "2.0"},
                                   {"method", "notifications/message"},
                                   {"params", {{"level", level}, {"data", message}}}}));
        }
    }

 private:
    std::map<std::string, std::unique_ptr<Camera>> cameras_;
};

}  // namespace mcp
