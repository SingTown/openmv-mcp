#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mcp {

struct Board {
    uint16_t vid;
    uint16_t pid;
    uint16_t pidMask;
    std::string boardType;
    std::string displayName;
    std::string archString;
};

extern const std::vector<Board> ALL_BOARDS;

inline const Board* findBoard(uint16_t vid, uint16_t pid) {
    for (const auto& b : ALL_BOARDS) {
        if (vid == b.vid && (pid & b.pidMask) == b.pid) return &b;
    }
    return nullptr;
}

extern const std::map<uint32_t, std::string> ALL_SENSORS_MAP;

}  // namespace mcp
