#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace mcp {

struct Board {
    uint16_t vid;
    uint16_t pid;
    uint16_t pidMask;
    std::string boardType;
    std::string name;
    std::string archString;
    std::string factoryDetectCommand;
    std::string factoryDetectMessage;
    std::string bootloaderDetectCommand;
    std::string bootloaderDetectMessage;
    std::vector<std::string> bootloaderCommands;
    std::vector<std::string> firmwareCommands;
    std::filesystem::path firmwareDir;
    bool checkLicense = false;
};

std::vector<Board> allBoards();

inline Board findBoardByVidPid(uint16_t vid, uint16_t pid) {
    for (const auto& b : allBoards()) {
        if (vid == b.vid && (pid & b.pidMask) == b.pid) return b;
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Unknown board: vid=0x%04X pid=0x%04X", vid, pid);
    throw std::runtime_error(buf);
}

inline Board findBoardByName(const std::string& name) {
    for (const auto& b : allBoards()) {
        if (b.name == name) return b;
    }
    throw std::runtime_error("Unknown board: " + name);
}

extern const std::map<uint32_t, std::string> ALL_SENSORS_MAP;

}  // namespace mcp
