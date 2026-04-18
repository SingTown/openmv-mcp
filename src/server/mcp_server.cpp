#include "server/mcp_server.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "openmv_version.h"

namespace mcp {

McpServer::McpServer(int port) : port_(port), ctx_(std::make_unique<McpContext>()) {}

McpServer::~McpServer() {
    stopping_->store(true);
    server_.stop();
}

void McpServer::setupStatusStream() {
    server_.Get("/stream/status", [this](const httplib::Request& req, httplib::Response& res) {
        auto camera_path = req.get_param_value("camera");
        std::shared_ptr<Camera> cam;
        try {
            cam = ctx_->getCamera(camera_path);
        } catch (const std::exception& e) {
            res.status = 404;
            res.set_content(json({{"error", std::string(e.what())}}).dump(), "application/json");
            return;
        }

        struct State {
            std::mutex mtx;
            std::condition_variable cv;
            json pending = json::object();
            bool has_pending = false;
            bool alive = true;
        };
        auto state = std::make_shared<State>();

        auto status_id = cam->onStatus([state](const Camera::Status& s) {
            {
                std::lock_guard<std::mutex> lk(state->mtx);
                state->pending = {{"connected", s.connected}, {"script_running", s.script_running}};
                state->has_pending = true;
            }
            state->cv.notify_all();
        });

        res.set_header("Cache-Control", "no-cache");
        res.set_header("X-Accel-Buffering", "no");
        res.set_chunked_content_provider(
            "text/event-stream",
            [state, stopping = stopping_](size_t, httplib::DataSink& sink) {
                json event;
                {
                    std::unique_lock<std::mutex> lk(state->mtx);
                    state->cv.wait_for(lk, std::chrono::milliseconds(200), [&] {
                        return state->has_pending || !state->alive || stopping->load();
                    });
                    if (!state->has_pending) {
                        if (!state->alive || stopping->load() || !sink.is_writable()) {
                            sink.done();
                            return false;
                        }
                        return true;
                    }
                    event = std::move(state->pending);
                    state->pending = json::object();
                    state->has_pending = false;
                }
                writeStreamEvent(sink.os, event);
                return true;
            },
            [cam, state, status_id](bool) {
                {
                    std::lock_guard<std::mutex> lk(state->mtx);
                    state->alive = false;
                }
                state->cv.notify_all();
                cam->removeCallback(status_id);
            });
    });
}

void McpServer::setupTerminalStream() {
    server_.Get("/stream/terminal", [this](const httplib::Request& req, httplib::Response& res) {
        auto camera_path = req.get_param_value("camera");
        std::shared_ptr<Camera> cam;
        try {
            cam = ctx_->getCamera(camera_path);
        } catch (const std::exception& e) {
            res.status = 404;
            res.set_content(json({{"error", std::string(e.what())}}).dump(), "application/json");
            return;
        }

        struct State {
            std::mutex mtx;
            std::condition_variable cv;
            std::string pending;
            bool alive = true;
        };
        auto state = std::make_shared<State>();

        auto cb_id = cam->onTerminal([state](const std::string& text) {
            {
                std::lock_guard<std::mutex> lk(state->mtx);
                state->pending.append(text);
            }
            state->cv.notify_one();
        });

        res.set_header("Cache-Control", "no-cache");
        res.set_header("X-Accel-Buffering", "no");
        res.set_chunked_content_provider(
            "text/plain; charset=utf-8",
            [state, stopping = stopping_](size_t, httplib::DataSink& sink) {
                std::string chunk;
                {
                    std::unique_lock<std::mutex> lk(state->mtx);
                    state->cv.wait_for(lk, std::chrono::milliseconds(200), [&] {
                        return !state->pending.empty() || !state->alive || stopping->load();
                    });
                    if (state->pending.empty()) {
                        if (!state->alive || stopping->load() || !sink.is_writable()) {
                            sink.done();
                            return false;
                        }
                        return true;
                    }
                    chunk.swap(state->pending);
                }
                return sink.write(chunk.data(), chunk.size());
            },
            [cam, state, cb_id](bool) {
                {
                    std::lock_guard<std::mutex> lk(state->mtx);
                    state->alive = false;
                }
                state->cv.notify_all();
                cam->removeCallback(cb_id);
            });
    });
}

void McpServer::setupMjpegStream() {
    server_.Get("/stream/frame", [this](const httplib::Request& req, httplib::Response& res) {
        auto camera_path = req.get_param_value("camera");
        std::shared_ptr<Camera> cam;
        try {
            cam = ctx_->getCamera(camera_path);
        } catch (const std::exception& e) {
            res.status = 404;
            res.set_content(json({{"error", std::string(e.what())}}).dump(), "application/json");
            return;
        }

        struct State {
            std::mutex mtx;
            std::condition_variable cv;
            std::shared_ptr<const Frame> frame;
            bool alive = true;
        };
        auto state = std::make_shared<State>();

        auto cb_id = cam->onFrame([state](const Frame& frame) {
            {
                std::lock_guard<std::mutex> lk(state->mtx);
                state->frame = std::make_shared<const Frame>(frame);
            }
            state->cv.notify_one();
        });

        res.set_chunked_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [state, stopping = stopping_](size_t, httplib::DataSink& sink) {
                std::shared_ptr<const Frame> frame;
                {
                    std::unique_lock<std::mutex> lk(state->mtx);
                    state->cv.wait_for(lk, std::chrono::milliseconds(200), [&] {
                        return state->frame || !state->alive || stopping->load();
                    });
                    if (!state->frame) {
                        if (!state->alive || stopping->load() || !sink.is_writable()) {
                            sink.done();
                            return false;
                        }
                        return true;
                    }
                    frame = std::move(state->frame);
                }
                auto jpeg = frame->toJpeg();
                std::string chunk =
                    "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(jpeg.size()) +
                    "\r\n\r\n";
                chunk.append(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
                chunk.append("\r\n");
                return sink.write(chunk.data(), chunk.size());
            },
            [cam, state, cb_id](bool) {
                {
                    std::lock_guard<std::mutex> lk(state->mtx);
                    state->alive = false;
                }
                state->cv.notify_all();
                cam->removeCallback(cb_id);
            });
    });
}

void McpServer::setupRoutes() {
    setupStatusStream();
    setupMjpegStream();
    setupTerminalStream();

    server_.Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
        spdlog::debug("HTTP POST /mcp request: {}", req.body);
        try {
            auto request = json::parse(req.body);
            auto method = request.value("method", "");

            if (method == "tools/call") {
                auto params = request.value("params", json::object());
                auto name = params.value("name", "");
                const auto* tool = findTool(name);

                if (tool != nullptr && tool->streaming) {
                    auto id = request.value("id", json(nullptr));
                    auto args = params.value("arguments", json::object());
                    auto meta = params.value("_meta", json::object());
                    auto progressToken = meta.value("progressToken", json(nullptr));
                    auto provider = [this,
                                     tool,
                                     id = std::move(id),
                                     args = std::move(args),
                                     progressToken = std::move(progressToken)](size_t, httplib::DataSink& sink) {
                        auto requestId = id.dump();
                        auto cancelled = ctx_->registerCancellation(requestId);
                        ctx_->stream = &sink.os;
                        ctx_->setProgressToken(progressToken);
                        try {
                            auto resp = tool->handler(*ctx_, args, *cancelled);
                            auto payload = makeResponse(id, {{"content", resp.toContent()}});
                            spdlog::debug("HTTP POST /mcp stream response: {}", payload.dump());
                            writeStreamEvent(sink.os, payload);
                        } catch (const std::exception& e) {
                            spdlog::error("tool '{}' threw: {}", tool->name, e.what());
                            McpContent err;
                            err.addText(json({{"error", std::string(e.what())}}));
                            auto payload = makeResponse(id, {{"content", err.toContent()}, {"isError", true}});
                            spdlog::debug("HTTP POST /mcp stream response: {}", payload.dump());
                            writeStreamEvent(sink.os, payload);
                        }
                        ctx_->stream = nullptr;
                        ctx_->setProgressToken(nullptr);
                        ctx_->unregisterCancellation(requestId);
                        sink.done();
                        return true;
                    };
                    res.set_chunked_content_provider("text/event-stream", std::move(provider));
                    return;
                }
            }

            res.set_header("Content-Type", "application/json");
            auto response = handleRequest(request);
            if (response.is_null()) {
                res.status = 202;
                spdlog::debug("HTTP POST /mcp response: 202 Accepted");
                return;
            }
            auto body = response.dump();
            spdlog::debug("HTTP POST /mcp response: {}", body);
            res.set_content(body, "application/json");
        } catch (const json::parse_error& e) {
            res.set_header("Content-Type", "application/json");
            auto err = makeError(nullptr, -32700, std::string("Parse error: ") + e.what());
            spdlog::debug("HTTP POST /mcp response: {}", err.dump());
            res.set_content(err.dump(), "application/json");
        }
    });

    server_.Post("/shutdown", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"stopping"})", "application/json");
        std::thread([this] { stop(); }).detach();
    });
}

bool McpServer::bind() {
    setupRoutes();
    server_.set_socket_options([]([[maybe_unused]] socket_t sock) {
#ifndef _WIN32
        int yes = 1;
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
#endif
    });
    return server_.bind_to_port("127.0.0.1", port_);
}

void McpServer::start() {
    spdlog::info("MCP Server listening on port {}", port_);
    server_.listen_after_bind();
    ctx_.reset();
}

void McpServer::stop() {
    stopping_->store(true);
    server_.stop();
}

json McpServer::handleRequest(const json& request) {
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return makeError(request.value("id", json(nullptr)), -32600, "Invalid JSON-RPC version");
    }

    auto method = request.value("method", "");
    auto id = request.value("id", json(nullptr));
    auto params = request.value("params", json::object());

    if (method == "initialize") {
        return handleInitialize(id);
    }
    if (method == "notifications/initialized") {
        return json(nullptr);
    }
    if (method == "notifications/cancelled") {
        auto requestId = params.value("requestId", json(nullptr));
        if (!requestId.is_null()) {
            ctx_->cancel(requestId.dump());
        }
        return json(nullptr);
    }
    if (method == "tools/list") {
        return handleToolsList(id);
    }
    if (method == "tools/call") {
        return handleToolsCall(params, id);
    }
    if (method == "ping") {
        return makeResponse(id, json::object());
    }

    return makeError(id, -32601, "Method not found: " + method);
}

json McpServer::handleInitialize(const json& id) {
    return makeResponse(id,
                        {{"protocolVersion", "2025-03-26"},
                         {"capabilities", {{"tools", json::object()}}},
                         {"serverInfo", {{"name", "openmv-mcp-server"}, {"version", OPENMV_MCP_VERSION}}}});
}

json McpServer::handleToolsList(const json& id) {
    static const json tools = [] {
        json arr = json::array();
        for (const auto* tool : ALL_MCP_TOOLS) {
            arr.push_back(
                {{"name", tool->name}, {"description", tool->description}, {"inputSchema", tool->input_schema}});
        }
        return arr;
    }();
    return makeResponse(id, {{"tools", tools}});
}

const McpTool* McpServer::findTool(const std::string& name) {
    auto it = std::find_if(
        ALL_MCP_TOOLS.begin(), ALL_MCP_TOOLS.end(), [&name](const auto* tool) { return tool->name == name; });
    if (it == ALL_MCP_TOOLS.end()) {
        return nullptr;
    }
    return *it;
}

json McpServer::handleToolsCall(const json& params, const json& id) {
    auto name = params.value("name", "");
    auto args = params.value("arguments", json::object());

    const auto* tool = findTool(name);
    if (tool == nullptr) {
        return makeError(id, -32602, "Unknown tool: " + name);
    }

    try {
        std::atomic<bool> no_cancel{false};
        auto resp = tool->handler(*ctx_, args, no_cancel);
        return makeResponse(id, {{"content", resp.toContent()}});
    } catch (const std::exception& e) {
        McpContent err;
        err.addText(json({{"error", std::string(e.what())}}));
        return makeResponse(id, {{"content", err.toContent()}, {"isError", true}});
    }
}

json McpServer::makeResponse(const json& id, const json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

json McpServer::makeError(const json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

}  // namespace mcp
