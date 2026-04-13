#include "firmware.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

#include "board.h"
#include "resource.h"
#include "subprocess/subprocess.h"

namespace mcp {

namespace fs = std::filesystem;

static fs::path resolveFirmwareDir(const fs::path& dir) {
    if (dir.empty()) {
        throw std::runtime_error("Firmware directory is not configured for this board");
    }
    if (dir.is_absolute()) {
        return dir;
    }
    return resourcePath() / dir;
}

static void waitForDevice(const std::string& detectCommand, const std::atomic<bool>& cancelled) {
    if (detectCommand.empty()) {
        return;
    }

    while (true) {
        try {
            Subprocess({detectCommand}, ".", nullptr, cancelled).join();
            return;
        } catch (const std::runtime_error&) {
            if (cancelled.load()) {
                throw;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

static void requireResourcePath() {
    if (resourcePath().empty()) {
        throw std::runtime_error("Resource path is not set. Use --resource-path to specify it.");
    }
}

void firmwareRepair(const std::string& name,
                    const MessageCallback& onNotice,
                    const MessageCallback& onDebug,
                    const std::atomic<bool>& cancelled) {
    requireResourcePath();
    const auto& board = findBoardByName(name);

    if (board.bootloaderCommands.empty() || board.firmwareCommands.empty()) {
        throw std::runtime_error("Board " + name + " does not support firmware repair");
    }

    const auto fwDir = resolveFirmwareDir(board.firmwareDir);

    if (onNotice && !board.factoryDetectMessage.empty()) {
        onNotice(board.factoryDetectMessage);
    }
    waitForDevice(board.factoryDetectCommand, cancelled);
    if (onNotice) {
        onNotice("Flashing bootloader...");
    }
    Subprocess(board.bootloaderCommands, fwDir, onDebug, cancelled).join();

    if (onNotice && !board.bootloaderDetectMessage.empty()) {
        onNotice(board.bootloaderDetectMessage);
    }
    waitForDevice(board.bootloaderDetectCommand, cancelled);
    if (onNotice) {
        onNotice("Flashing firmware...");
    }
    Subprocess(board.firmwareCommands, fwDir, onDebug, cancelled).join();
}

void firmwareFlash(const std::string& name,
                   const fs::path& firmwareDir,
                   const MessageCallback& onNotice,
                   const MessageCallback& onDebug,
                   const std::atomic<bool>& cancelled) {
    requireResourcePath();
    const auto& board = findBoardByName(name);

    if (board.firmwareCommands.empty()) {
        throw std::runtime_error("Board " + name + " does not support firmware flashing");
    }

    waitForDevice(board.bootloaderDetectCommand, cancelled);

    if (onNotice) {
        onNotice("Flashing firmware...");
    }
    auto dir = firmwareDir.empty() ? board.firmwareDir : firmwareDir;
    Subprocess(board.firmwareCommands, resolveFirmwareDir(dir), onDebug, cancelled).join();
}

std::string latestFirmwareVersion() {
    static std::mutex mutex;
    static std::string cached;
    std::lock_guard<std::mutex> lock(mutex);
    if (!cached.empty()) {
        return cached;
    }
    if (resourcePath().empty()) {
        return {};
    }
    std::ifstream in(resourcePath() / "firmware" / "settings.json");
    if (!in) {
        return {};
    }
    auto j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded()) {
        return {};
    }
    auto it = j.find("firmware_version");
    if (it == j.end() || !it->is_string()) {
        return {};
    }
    cached = it->get<std::string>();
    return cached;
}

}  // namespace mcp
