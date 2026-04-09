#include "firmware.h"

#include <chrono>
#include <filesystem>
#include <thread>

#include "board.h"
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

static void checkCancelled(const std::atomic<bool>& cancelled) {
    if (cancelled.load()) {
        throw std::runtime_error("Operation cancelled");
    }
}

static void waitForDevice(const std::string& detectCommand, const std::atomic<bool>& cancelled) {
    if (detectCommand.empty()) {
        return;
    }

    while (true) {
        checkCancelled(cancelled);
        Subprocess probe({detectCommand});
        probe.join();
        if (probe.exitCode() == 0) {
            return;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

static void runCommands(const std::vector<std::string>& commands,
                        const MessageCallback& onDebug,
                        const fs::path& cwd,
                        const std::atomic<bool>& cancelled) {
    Subprocess proc(commands, cwd);

    while (!proc.finished()) {
        checkCancelled(cancelled);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto output = proc.readOutput();
        if (!output.empty() && onDebug) {
            onDebug(output);
        }
    }

    auto remaining = proc.readOutput();
    if (!remaining.empty() && onDebug) {
        onDebug(remaining);
    }

    if (proc.exitCode() != 0) {
        throw std::runtime_error("Command failed with exit code " + std::to_string(proc.exitCode()));
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
    runCommands(board.bootloaderCommands, onDebug, fwDir, cancelled);

    if (onNotice && !board.bootloaderDetectMessage.empty()) {
        onNotice(board.bootloaderDetectMessage);
    }
    waitForDevice(board.bootloaderDetectCommand, cancelled);
    if (onNotice) {
        onNotice("Flashing firmware...");
    }
    runCommands(board.firmwareCommands, onDebug, fwDir, cancelled);
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
    runCommands(board.firmwareCommands, onDebug, resolveFirmwareDir(dir), cancelled);
}

}  // namespace mcp
