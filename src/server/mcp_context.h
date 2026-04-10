#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "camera.h"

namespace mcp {

using json = nlohmann::ordered_json;

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

    Camera& addCamera(const std::string& path, std::unique_ptr<Camera> camera) {
        if (cameras_.count(path) != 0) {
            throw std::runtime_error("Camera already connected: " + path);
        }
        auto& ref = cameras_[path] = std::move(camera);
        return *ref;
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

    std::shared_ptr<std::atomic<bool>> registerCancellation(const std::string& key) {
        auto flag = std::make_shared<std::atomic<bool>>(false);
        std::lock_guard lock(cancellation_mutex_);
        cancellations_[key] = flag;
        return flag;
    }

    void unregisterCancellation(const std::string& key) {
        std::lock_guard lock(cancellation_mutex_);
        cancellations_.erase(key);
    }

    void cancel(const std::string& key) {
        std::lock_guard lock(cancellation_mutex_);
        auto it = cancellations_.find(key);
        if (it != cancellations_.end()) {
            it->second->store(true);
        }
    }

 private:
    std::map<std::string, std::unique_ptr<Camera>> cameras_;
    std::mutex cancellation_mutex_;
    std::unordered_map<std::string, std::shared_ptr<std::atomic<bool>>> cancellations_;
};

}  // namespace mcp
