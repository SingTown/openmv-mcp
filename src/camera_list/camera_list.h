#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mcp {

struct PortInfo {
    std::string path;
    std::string name;
};

// List connected OpenMV cameras (filtered from all serial ports)
std::vector<PortInfo> listCameras();

// Find the USB mass-storage drive path for a given serial port path
std::filesystem::path findDrivePath(const std::string& serialPath);

}  // namespace mcp
