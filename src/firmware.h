#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>

namespace mcp {

using MessageCallback = std::function<void(const std::string&)>;

void firmwareRepair(const std::string& name, const MessageCallback& onMessage, const std::atomic<bool>& cancelled);
void firmwareFlash(const std::string& name,
                   const std::filesystem::path& firmwareDir,
                   const MessageCallback& onMessage,
                   const std::atomic<bool>& cancelled);

std::string latestFirmwareVersion();

}  // namespace mcp
