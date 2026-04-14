#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

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
        std::lock_guard lock(cameras_mutex_);
        auto it = cameras_.find(path);
        if (it == cameras_.end()) {
            throw std::runtime_error("Camera not connected: " + path);
        }
        return *it->second;
    }

    [[nodiscard]] bool hasCamera(const std::string& path) const {
        std::lock_guard lock(cameras_mutex_);
        return cameras_.contains(path);
    }

    Camera& addCamera(const std::string& path, std::unique_ptr<Camera> camera) {
        std::lock_guard lock(cameras_mutex_);
        if (cameras_.count(path) != 0) {
            throw std::runtime_error("Camera already connected: " + path);
        }
        auto& ref = cameras_[path] = std::move(camera);
        return *ref;
    }

    void removeCamera(const std::string& path) {
        auto cam = takeCamera(path);
        cam->disconnect();
    }

    std::unique_ptr<Camera> takeCamera(const std::string& path) {
        std::lock_guard lock(cameras_mutex_);
        auto it = cameras_.find(path);
        if (it == cameras_.end()) {
            throw std::runtime_error("Camera not connected: " + path);
        }
        auto cam = std::move(it->second);
        cameras_.erase(it);
        return cam;
    }

    void setProgressToken(json token) { progress_token_ = std::move(token); }

    void streamProgress(uint64_t progress, std::optional<uint64_t> total, const std::string& message) {
        if (stream == nullptr || progress_token_.is_null()) {
            return;
        }
        json params = {
            {"progressToken", progress_token_},
            {"progress", progress},
        };
        if (total.has_value()) {
            params["total"] = *total;
        }
        if (!message.empty()) {
            params["message"] = message;
        }
        writeStreamEvent(
            *stream, json({{"jsonrpc", "2.0"}, {"method", "notifications/progress"}, {"params", std::move(params)}}));
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
    mutable std::mutex cameras_mutex_;
    std::map<std::string, std::unique_ptr<Camera>> cameras_;
    std::mutex cancellation_mutex_;
    std::unordered_map<std::string, std::shared_ptr<std::atomic<bool>>> cancellations_;
    json progress_token_;
};

}  // namespace mcp
