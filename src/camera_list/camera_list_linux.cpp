#include "camera_list.h"

#ifdef __linux__

namespace mcp {

std::vector<PortInfo> listCameras() {
    return {};
}

}  // namespace mcp

#endif  // __linux__
