#pragma once

#include <string>
#include <vector>

namespace mcp {

struct PortInfo {
    std::string path;
    std::string name;
};

// List connected OpenMV cameras (filtered from all serial ports)
std::vector<PortInfo> listCameras();

}  // namespace mcp
