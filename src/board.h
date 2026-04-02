#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mcp {

struct Board {
    uint16_t vid;
    uint16_t pid;
    uint16_t pidMask;
    std::string displayName;
};

extern const std::vector<Board> ALL_BOARDS;

}  // namespace mcp
