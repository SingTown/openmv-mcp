#pragma once

#include "mcp_device_test.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

struct WsReader {
    httplib::ws::WebSocketClient ws;
    std::thread thread;
    std::mutex mu;
    std::vector<std::string> messages;
    std::atomic<bool> done{false};

    explicit WsReader(const std::string& url) : ws(url) {
        ws.set_read_timeout(30);
        ws.set_connection_timeout(5);
    }

    bool start() {
        if (!ws.connect()) return false;
        thread = std::thread([this]() {
            while (!done.load()) {
                std::string msg;
                auto r = ws.read(msg);
                if (r == httplib::ws::ReadResult::Fail) break;
                std::lock_guard<std::mutex> lock(mu);
                messages.push_back(std::move(msg));
            }
        });
        return true;
    }

    bool waitFor(const std::function<bool(const std::string&)>& pred, int timeout_ms = 10000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mu);
                for (const auto& m : messages) {
                    if (pred(m)) return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

    bool waitForJson(const std::function<bool(const json&)>& pred, int timeout_ms = 10000) {
        return waitFor(
            [&pred](const std::string& m) {
                auto j = json::parse(m, nullptr, false);
                return j.is_object() && pred(j);
            },
            timeout_ms);
    }

    bool waitForString(const std::string& substring, int timeout_ms = 10000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mu);
                std::string all;
                for (const auto& m : messages) {
                    all += m;
                }
                if (all.find(substring) != std::string::npos) return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

    void stop() {
        done.store(true);
        ws.close();
        if (thread.joinable()) thread.join();
    }

    ~WsReader() { stop(); }
};
