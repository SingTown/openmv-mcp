#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mcp_device_test.h"

// =======================================================================
// Streaming endpoint integration tests
// =======================================================================

namespace {

struct SseReader {
    httplib::Client http;
    std::thread thread;
    std::mutex mu;
    std::vector<json> events;
    std::string buf;
    std::atomic<bool> stopped{false};

    SseReader(const std::string& host, int port) : http(host, port) { http.set_read_timeout(30, 0); }

    void start(const std::string& path) {
        thread = std::thread([this, path]() {
            http.Get(path, [this](const char* data, size_t len) {
                buf.append(data, len);
                size_t pos;
                while ((pos = buf.find("\n\n")) != std::string::npos) {
                    std::string frame = buf.substr(0, pos);
                    buf.erase(0, pos + 2);
                    size_t dpos = frame.find("data:");
                    if (dpos == std::string::npos) {
                        continue;
                    }
                    std::string payload = frame.substr(dpos + 5);
                    while (!payload.empty() && (payload.front() == ' ' || payload.front() == '\t')) {
                        payload.erase(0, 1);
                    }
                    auto j = json::parse(payload, nullptr, false);
                    if (j.is_object()) {
                        std::lock_guard<std::mutex> lock(mu);
                        events.push_back(std::move(j));
                    }
                }
                return !stopped.load();
            });
        });
    }

    bool waitForJson(const std::function<bool(const json&)>& pred, int timeout_ms = 10000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mu);
                for (const auto& j : events) {
                    if (pred(j)) {
                        return true;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

    void stop() {
        stopped.store(true);
        http.stop();
        if (thread.joinable()) {
            thread.join();
        }
    }

    ~SseReader() { stop(); }
};

}  // namespace

TEST_F(DeviceTest, StatusStream) {
    SseReader reader("127.0.0.1", kPort);
    reader.start("/stream/status?camera=" + camera_path_);

    EXPECT_TRUE(reader.waitForJson([](const json& j) {
        return j.contains("connected") && j.contains("script_running");
    })) << "Did not receive initial status";

    client_
        ->callTool("script_run",
                   {{"cameraPath", camera_path_}, {"script", "import time\nwhile True:\n    time.sleep_ms(100)\n"}})
        .wait();

    EXPECT_TRUE(reader.waitForJson([](const json& j) { return j.value("script_running", false); }))
        << "Did not receive script_running=true";

    client_->callTool("script_stop", {{"cameraPath", camera_path_}}).wait();

    EXPECT_TRUE(reader.waitForJson([](const json& j) {
        return j.contains("script_running") && !j.value("script_running", true);
    })) << "Did not receive script_running=false";
}

TEST_F(DeviceTest, HttpTerminalStream) {
    httplib::Client http("127.0.0.1", kPort);
    http.set_read_timeout(15, 0);

    std::atomic<bool> got_text{false};
    std::string buf;
    std::thread reader([&] {
        http.Get("/stream/terminal?camera=" + camera_path_, [&](const char* data, size_t len) {
            buf.append(data, len);
            if (buf.find("ws_hello_test") != std::string::npos) {
                got_text.store(true);
                return false;
            }
            return true;
        });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client_->callTool("script_run", {{"cameraPath", camera_path_}, {"script", "print('ws_hello_test')"}}).wait();

    for (int i = 0; i < 100 && !got_text.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    http.stop();
    if (reader.joinable()) {
        reader.join();
    }

    EXPECT_TRUE(got_text.load()) << "Did not receive expected terminal output via HTTP stream";
}

TEST_F(DeviceTest, HttpFrameStream) {
    client_->callTool("script_run", {{"cameraPath", camera_path_}, {"script", kSnapshotScript}}).wait();

    httplib::Client http("127.0.0.1", kPort);
    http.set_read_timeout(15, 0);

    std::atomic<bool> got_jpeg{false};
    std::string buf;
    auto res = http.Get("/stream/frame?camera=" + camera_path_, [&](const char* data, size_t len) {
        buf.append(data, len);
        for (size_t i = 0; i + 1 < buf.size(); ++i) {
            if (static_cast<uint8_t>(buf[i]) == 0xFF && static_cast<uint8_t>(buf[i + 1]) == 0xD8) {
                got_jpeg.store(true);
                return false;
            }
        }
        return true;
    });

    ASSERT_TRUE(got_jpeg.load()) << "No JPEG frame received via HTTP MJPEG stream";
}

TEST_F(DeviceTest, ReadFrame) {
    auto run_result =
        client_->callTool("script_run", {{"cameraPath", camera_path_}, {"script", kSnapshotScript}}).wait();
    ASSERT_FALSE(run_result.is_error);

    mcp::ContentItem frame_content;
    std::string last_error;
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto result = client_->callTool("frame_capture", {{"cameraPath", camera_path_}}).wait();
        if (result.content.empty()) continue;
        if (result.is_error) {
            last_error = result.content[0].text;
            continue;
        }
        if (result.content[0].type != "image") continue;
        if (!result.content[0].data.empty()) {
            frame_content = result.content[0];
            break;
        }
    }
    if (frame_content.type.empty() && !last_error.empty()) {
        std::cout << "Last read_frame error: " << last_error << "\n";
    }

    ASSERT_FALSE(frame_content.type.empty()) << "Failed to read a frame within timeout";
    EXPECT_EQ(frame_content.type, "image");
    EXPECT_EQ(frame_content.mime_type, "image/jpeg");
    EXPECT_FALSE(frame_content.data.empty());
    std::cout << "Frame base64 size: " << frame_content.data.size() << " bytes\n";
}
