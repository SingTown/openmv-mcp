#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>

namespace mcp {

using MessageCallback = std::function<void(const std::string&)>;

void firmwareRepair(const std::string& name,
                    const MessageCallback& onNotice,
                    const MessageCallback& onDebug,
                    const std::atomic<bool>& cancelled);
void firmwareFlash(const std::string& name,
                   const std::filesystem::path& firmwareDir,
                   const MessageCallback& onNotice,
                   const MessageCallback& onDebug,
                   const std::atomic<bool>& cancelled);

}  // namespace mcp
