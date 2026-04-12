#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>

#include "daemonize.h"
#include "resource.h"
#include "server/mcp_server.h"

static std::atomic<mcp::McpServer*> g_server{nullptr};

static void signalHandler(int /*sig*/) {
    if (auto* s = g_server.load(std::memory_order_acquire)) {
        s->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = 15257;
    bool daemon_mode = false;
    std::string log_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try {
                port = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid port number: " << argv[i] << '\n';
                return 1;
            }
        } else if (arg == "--daemon" || arg == "-d") {
            daemon_mode = true;
        } else if (arg == "--log" && i + 1 < argc) {
            log_path = argv[++i];
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
                      << "  --daemon, -d       Run in background (POSIX only)\n"
                      << "  --log <path>       Redirect stdout/stderr to file in daemon mode\n"
                      << "  --version, -v      Print version and exit\n";
            return 0;
        }
    }

    mcp::extractEmbeddedResource();

    mcp::McpServer server(port);
    if (!server.bind()) {
        std::cerr << "Failed to bind port " << port << " (already in use?)\n";
        return 1;
    }

    if (daemon_mode) {
        try {
            mcp::daemonize(log_path);
        } catch (const std::exception& e) {
            std::cerr << "daemonize failed: " << e.what() << '\n';
            return 1;
        }
    }

    g_server.store(&server, std::memory_order_release);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    server.start();
    return 0;
}
