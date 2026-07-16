#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <csignal>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "detached_process.h"
#include "openmv_version.h"
#include "server/mcp_server.h"
#include "stdio_proxy.h"

static std::atomic<mcp::McpServer*> g_server{nullptr};
static constexpr int kDefaultPort = 15257;
static constexpr const char* kLogFileName = "openmv-mcp-server-log.txt";

static std::string defaultLogPath() {
    std::error_code ec;
    const auto temp_dir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return kLogFileName;
    }
    return (temp_dir / kLogFileName).string();
}

struct CommandLineOptions {
    int port = kDefaultPort;
    std::string mode;
    bool show_help = false;
    bool show_version = false;
    std::string log_path;
    std::string log_level;
    spdlog::level::level_enum log_level_value = spdlog::level::info;
    std::string help_text;
};

static CommandLineOptions parseCommandLine(int argc, char* argv[]) {
    cxxopts::Options parser("openmv_mcp_server", "OpenMV MCP server");
    parser.custom_help("[OPTIONS]");

    auto add_option = parser.add_options();
    add_option("p,port", "HTTP port", cxxopts::value<int>()->default_value(std::to_string(kDefaultPort)), "<port>");
    add_option("mode", "Run mode: shutdown|stdio|internal_server", cxxopts::value<std::string>(), "<mode>");
    add_option("log",
               "Write HTTP server logs to file",
               cxxopts::value<std::string>()->default_value(defaultLogPath()),
               "<path>");
    add_option("level",
               "Log level: trace|debug|info|warn|error|critical|off",
               cxxopts::value<std::string>()->default_value("info"),
               "<lvl>");
    add_option("v,version", "Print version and exit");
    add_option("h,help", "Print help and exit");

    auto parsed = parser.parse(argc, argv);

    CommandLineOptions options;
    options.show_help = parsed["help"].as<bool>();
    options.show_version = parsed["version"].as<bool>();
    options.help_text = parser.help({""});
    if (options.show_help || options.show_version) {
        return options;
    }

    options.port = parsed["port"].as<int>();
    if (parsed.count("mode") > 0) {
        options.mode = parsed["mode"].as<std::string>();
    }
    options.log_path = parsed["log"].as<std::string>();

    const auto level = parsed["level"].as<std::string>();
    options.log_level_value = spdlog::level::from_str(level);
    if (options.log_level_value == spdlog::level::off && level != "off") {
        throw std::runtime_error("Invalid log level: " + level +
                                 " (expected: trace|debug|info|warn|error|critical|off)");
    }
    if (parsed.count("level") > 0) {
        options.log_level = level;
    }

    return options;
}

static void signalHandler(int /*sig*/) {
    if (auto* s = g_server.load(std::memory_order_acquire)) {
        s->stop();
    }
}

int main(int argc, char* argv[]) {
    spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$] %v");
    spdlog::set_level(spdlog::level::info);

    CommandLineOptions options;
    try {
        options = parseCommandLine(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    if (options.show_help) {
        std::cout << options.help_text;
        return 0;
    }

    if (options.show_version) {
        std::cout << OPENMV_MCP_VERSION << '\n';
        return 0;
    }

    if (options.mode.empty()) {
        try {
            mcp::ensureServerRunning(argv[0], options.port, options.log_path, options.log_level);
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            return 1;
        }
        return 0;
    }

    if (options.mode == "stdio") {
        spdlog::set_level(spdlog::level::off);
        try {
            mcp::ensureServerRunning(argv[0], options.port, options.log_path, options.log_level);
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            return 1;
        }
        return mcp::runStdioProxy("127.0.0.1", options.port);
    }

    if (options.mode == "shutdown") {
        try {
            mcp::shutdownServer(options.port);
            spdlog::info("Stopped openmv-mcp server on port {}", options.port);
            return 0;
        } catch (const std::exception& e) {
            spdlog::error("{}", e.what());
            return 1;
        }
    }

    if (options.mode != "internal_server") {
        std::cerr << "Invalid mode: " << options.mode << " (expected: shutdown|stdio|internal_server)\n";
        return 1;
    }

    if (!options.log_path.empty()) {
        auto logger = spdlog::basic_logger_mt("openmv-mcp", options.log_path, true);
        spdlog::set_default_logger(logger);
    }
    spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$] %v");
    spdlog::set_level(options.log_level_value);

    mcp::McpServer server(options.port);
    if (!server.bind()) {
        spdlog::error("Failed to bind port {} (already in use?)", options.port);
        return 1;
    }

    g_server.store(&server, std::memory_order_release);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    server.start();
    return 0;
}
