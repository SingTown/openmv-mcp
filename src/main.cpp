#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>

#include "server/mcp_server.h"

static std::atomic<mcp::McpServer*> g_server{nullptr};

static void signalHandler(int /*sig*/) {
    if (auto* s = g_server.load(std::memory_order_acquire)) {
        s->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = 15257;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try {
                port = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid port number: " << argv[i] << '\n';
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: openmv_mcp_server [--port PORT]\n"
                      << "  --port, -p  HTTP port (default: 15257)\n";
            return 0;
        }
    }

    mcp::McpServer server(port);
    g_server.store(&server, std::memory_order_release);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    server.start();
    return 0;
}
