#include "camera_list.h"

#ifdef __linux__

namespace mcp {

std::vector<PortInfo> listCameras() {
    return {};
}

std::filesystem::path findDrivePath(const std::string& /*serialPath*/) {
    return {};
}

}  // namespace mcp

#endif  // __linux__
