#include "camera_list.h"

#ifdef _WIN32

namespace mcp {

std::vector<PortInfo> listCameras() {
    return {};
}

}  // namespace mcp

#endif  // _WIN32
