#include "camera_list.h"

#ifdef _WIN32

// clang-format off
#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>
// clang-format on

#include <cstdlib>
#include <string>

#include "board.h"

namespace mcp {

static bool parseVidPid(const std::string& hwId, uint16_t& vid, uint16_t& pid) {
    auto vidPos = hwId.find("VID_");
    if (vidPos == std::string::npos || vidPos + 8 > hwId.size()) {
        return false;
    }
    auto pidPos = hwId.find("PID_");
    if (pidPos == std::string::npos || pidPos + 8 > hwId.size()) {
        return false;
    }

    char* end = nullptr;
    unsigned long v = std::strtoul(hwId.c_str() + vidPos + 4, &end, 16);
    if (end != hwId.c_str() + vidPos + 8) {
        return false;
    }
    unsigned long p = std::strtoul(hwId.c_str() + pidPos + 4, &end, 16);
    if (end != hwId.c_str() + pidPos + 8) {
        return false;
    }

    vid = static_cast<uint16_t>(v);
    pid = static_cast<uint16_t>(p);
    return true;
}

static std::string getDeviceRegistryString(HDEVINFO devInfo, SP_DEVINFO_DATA& devInfoData, DWORD property) {
    char buf[512] = {};
    DWORD type = 0;
    DWORD size = sizeof(buf);
    if (!SetupDiGetDeviceRegistryPropertyA(
            devInfo, &devInfoData, property, &type, reinterpret_cast<BYTE*>(buf), size, &size)) {
        return {};
    }
    if (type != REG_SZ && type != REG_MULTI_SZ) {
        return {};
    }
    return std::string(buf);
}

static std::string getPortName(HDEVINFO devInfo, SP_DEVINFO_DATA& devInfoData) {
    HKEY hKey = SetupDiOpenDevRegKey(devInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (hKey == INVALID_HANDLE_VALUE) {
        return {};
    }

    char buf[256] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    LONG rc = RegQueryValueExA(hKey, "PortName", nullptr, &type, reinterpret_cast<BYTE*>(buf), &size);
    RegCloseKey(hKey);

    if (rc != ERROR_SUCCESS || type != REG_SZ) {
        return {};
    }
    return std::string(buf);
}

std::vector<PortInfo> listCameras() {
    std::vector<PortInfo> result;

    HDEVINFO devInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return result;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); ++i) {
        std::string hwId = getDeviceRegistryString(devInfo, devInfoData, SPDRP_HARDWAREID);
        if (hwId.empty()) {
            continue;
        }

        uint16_t vid = 0;
        uint16_t pid = 0;
        if (!parseVidPid(hwId, vid, pid)) {
            continue;
        }

        try {
            const auto& board = findBoardByVidPid(vid, pid);
            std::string portName = getPortName(devInfo, devInfoData);
            if (!portName.empty()) {
                result.push_back({portName, board.name});
            }
        } catch (const std::runtime_error&) {
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return result;
}

}  // namespace mcp

#endif  // _WIN32
