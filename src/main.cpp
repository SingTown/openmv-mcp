#include <httplib/httplib.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>

#include "client/mcp_client.h"
#include "daemonize.h"
#include "resource.h"
#include "server/mcp_server.h"

static std::atomic<mcp::McpServer*> g_server{nullptr};

static bool isOpenmvMcpOnPort(int port) {
    try {
        mcp::McpClient client("127.0.0.1", port);
        auto info = client.initialize();
        return info.value("serverInfo", mcp::json::object()).value("name", "") == "openmv-mcp-server";
    } catch (const std::exception&) {
        return false;
    }
}

static void signalHandler(int /*sig*/) {
    if (auto* s = g_server.load(std::memory_order_acquire)) {
        s->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = 15257;
    bool daemon_mode = false;
    bool shutdown_mode = false;
    std::string log_path;

    spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$] %v");
    spdlog::set_level(spdlog::level::info);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try {
                port = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                spdlog::error("Invalid port number: {}", argv[i]);
                return 1;
            }
        } else if (arg == "--daemon" || arg == "-d") {
            daemon_mode = true;
        } else if (arg == "--shutdown") {
            shutdown_mode = true;
        } else if (arg == "--log" && i + 1 < argc) {
            log_path = argv[++i];
        } else if (arg == "--level" && i + 1 < argc) {
            std::string lv_str = argv[++i];
            auto lv = spdlog::level::from_str(lv_str);
            if (lv == spdlog::level::off && lv_str != "off") {
                spdlog::error("Invalid log level: {} (expected: trace|debug|info|warn|error|critical|off)", lv_str);
                return 1;
            }
            spdlog::set_level(lv);
        } else if (arg == "--version" || arg == "-v") {
#ifdef OPENMV_MCP_VERSION
            std::cout << OPENMV_MCP_VERSION << '\n';
#else
            std::cout << "unknown\n";
#endif
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: openmv_mcp_server [OPTIONS]\n"
                      << "  --port, -p <port>  HTTP port (default: 15257)\n"
                      << "  --daemon, -d       Run in background\n"
                      << "  --shutdown         Stop the running server on the given port\n"
                      << "  --log <path>       Redirect stdout/stderr to file in daemon mode\n"
                      << "  --level <lvl>      Log level: trace|debug|info|warn|error|critical|off (default: info)\n"
                      << "  --version, -v      Print version and exit\n";
            return 0;
        }
    }

    if (shutdown_mode) {
        if (!isOpenmvMcpOnPort(port)) {
            spdlog::info("No openmv-mcp server running on port {}", port);
            return 0;
        }
        httplib::Client cli("127.0.0.1", port);
        auto resp = cli.Post("/shutdown", "", "application/json");
        if (resp && resp->status == 200) {
            spdlog::info("Stopped openmv-mcp server on port {}", port);
            return 0;
        }
        spdlog::error("Failed to stop server on port {}", port);
        return 1;
    }

    mcp::extractEmbeddedResource();

    mcp::McpServer server(port);
    if (!server.bind()) {
        if (isOpenmvMcpOnPort(port)) {
            spdlog::info("openmv-mcp server already running on port {}", port);
            return 0;
        }
        spdlog::error("Failed to bind port {} (already in use?)", port);
        return 1;
    }

    if (daemon_mode) {
        try {
            mcp::daemonize(argc, argv, log_path);
        } catch (const std::exception& e) {
            spdlog::error("daemonize failed: {}", e.what());
            return 1;
        }
    }

    g_server.store(&server, std::memory_order_release);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    server.start();
    return 0;
}
