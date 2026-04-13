#include "camera_list.h"

#ifdef _WIN32

// clang-format off
#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <winioctl.h>
// clang-format on

#include <cstdlib>
#include <string>
#include <vector>

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

static std::string findUsbCompositeAncestorId(DEVINST devInst) {
    DEVINST current = devInst;
    for (int depth = 0; depth < 10; ++depth) {
        DEVINST parent = 0;
        if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS) {
            break;
        }

        char devId[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDA(parent, devId, sizeof(devId), 0) != CR_SUCCESS) {
            break;
        }

        std::string id(devId);
        // A USB composite device ID starts with "USB\VID_" but does NOT contain "&MI_"
        if (id.find("USB\\VID_") == 0 && id.find("&MI_") == std::string::npos) {
            return id;
        }

        current = parent;
    }
    return {};
}

static DEVINST findComPortDevInst(const std::string& comPort) {
    HDEVINFO devInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DEVINST result = 0;
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData) != FALSE; ++i) {
        if (getPortName(devInfo, devInfoData) == comPort) {
            result = devInfoData.DevInst;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return result;
}

// A COM port and its mass-storage sibling share a USB composite ancestor.
// Given that ancestor's device ID, find the sibling disk's device interface path.
static std::string findSiblingDiskPath(const std::string& targetAncestorId) {
    HDEVINFO devInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_DISKDRIVE, nullptr, nullptr, DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::string result;
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData) != FALSE; ++i) {
        std::string ancestorId = findUsbCompositeAncestorId(devInfoData.DevInst);
        if (ancestorId != targetAncestorId) {
            continue;
        }

        // Found the sibling disk — now get its device interface path via a separate device info set
        char devInstanceId[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDA(devInfoData.DevInst, devInstanceId, sizeof(devInstanceId), 0) != CR_SUCCESS) {
            continue;
        }

        HDEVINFO ifDevInfo = SetupDiGetClassDevsA(
            &GUID_DEVINTERFACE_DISK, devInstanceId, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (ifDevInfo == INVALID_HANDLE_VALUE) {
            continue;
        }

        SP_DEVICE_INTERFACE_DATA ifData;
        ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        if (SetupDiEnumDeviceInterfaces(ifDevInfo, nullptr, &GUID_DEVINTERFACE_DISK, 0, &ifData) != FALSE) {
            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetailA(ifDevInfo, &ifData, nullptr, 0, &requiredSize, nullptr);
            if (requiredSize > 0) {
                std::vector<char> detailBuf(requiredSize);
                auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(detailBuf.data());
                detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

                if (SetupDiGetDeviceInterfaceDetailA(ifDevInfo, &ifData, detail, requiredSize, nullptr, nullptr) !=
                    FALSE) {
                    result = detail->DevicePath;
                }
            }
        }

        SetupDiDestroyDeviceInfoList(ifDevInfo);
        break;
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return result;
}

static DWORD getDeviceNumber(const std::string& devicePath) {
    HANDLE hDisk =
        CreateFileA(devicePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDisk == INVALID_HANDLE_VALUE) {
        return ~0UL;
    }

    STORAGE_DEVICE_NUMBER sdn = {};
    DWORD bytesReturned = 0;
    DWORD number = ~0UL;
    if (DeviceIoControl(
            hDisk, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &sdn, sizeof(sdn), &bytesReturned, nullptr) != FALSE) {
        number = sdn.DeviceNumber;
    }
    CloseHandle(hDisk);
    return number;
}

static std::string findDriveLetter(DWORD deviceNumber) {
    char volumeName[MAX_PATH] = {};
    HANDLE hFind = FindFirstVolumeA(volumeName, MAX_PATH);
    if (hFind == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::string result;
    do {
        // Remove trailing backslash to open the volume device
        std::string volPath(volumeName);
        if (!volPath.empty() && volPath.back() == '\\') {
            volPath.pop_back();
        }

        HANDLE hVol =
            CreateFileA(volPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hVol == INVALID_HANDLE_VALUE) {
            continue;
        }

        STORAGE_DEVICE_NUMBER sdn = {};
        DWORD bytesReturned = 0;
        bool matched =
            DeviceIoControl(
                hVol, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &sdn, sizeof(sdn), &bytesReturned, nullptr) !=
                FALSE &&
            sdn.DeviceNumber == deviceNumber;
        CloseHandle(hVol);

        if (matched) {
            char pathBuf[MAX_PATH] = {};
            DWORD returnLength = 0;
            if (GetVolumePathNamesForVolumeNameA(volumeName, pathBuf, MAX_PATH, &returnLength) != FALSE &&
                pathBuf[0] != '\0') {
                result = pathBuf;
            }
            break;
        }
    } while (FindNextVolumeA(hFind, volumeName, MAX_PATH) != FALSE);

    FindVolumeClose(hFind);
    return result;
}

std::filesystem::path findDrivePath(const std::string& serialPath) {
    DEVINST comDevInst = findComPortDevInst(serialPath);
    if (comDevInst == 0) {
        return {};
    }

    std::string ancestorId = findUsbCompositeAncestorId(comDevInst);
    if (ancestorId.empty()) {
        return {};
    }

    std::string diskPath = findSiblingDiskPath(ancestorId);
    if (diskPath.empty()) {
        return {};
    }

    DWORD devNum = getDeviceNumber(diskPath);
    if (devNum == ~0UL) {
        return {};
    }

    std::string driveLetter = findDriveLetter(devNum);
    if (driveLetter.empty()) {
        return {};
    }

    return std::filesystem::path(driveLetter);
}

}  // namespace mcp

#endif  // _WIN32
