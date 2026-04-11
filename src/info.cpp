#include "info.h"

#include <httplib/httplib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <stdexcept>

#include "board.h"

namespace mcp {

static constexpr const char* kLicenseHost = "upload.openmv.io";
static constexpr int kLicenseTimeoutSec = 3;

static httplib::Result postForm(const std::string& path, const httplib::Params& params) {
    httplib::SSLClient cli(kLicenseHost);
    cli.set_connection_timeout(kLicenseTimeoutSec);
    cli.set_read_timeout(kLicenseTimeoutSec);
    cli.enable_server_certificate_verification(true);
    auto res = cli.Post(path, params);
    if (!res) {
        throw std::runtime_error("License server request failed: " + httplib::to_string(res.error()));
    }
    if (res->status != 200) {
        throw std::runtime_error("License server returned HTTP " + std::to_string(res->status));
    }
    return res;
}

CameraInfo::CameraInfo(uint16_t vid, uint16_t pid) {
    try {
        board_ = findBoardByVidPid(vid, pid);
    } catch (const std::runtime_error&) {
        board_ = {};
        board_.vid = vid;
        board_.pid = pid;
    }
}

void CameraInfo::setSensorChipId(const std::array<uint32_t, 3>& ids) {
    sensor_chip_id_ = ids;
    sensor_string_.clear();
    for (auto id : ids) {
        if (id == 0) continue;
        auto it = ALL_SENSORS_MAP.find(id);
        if (it != ALL_SENSORS_MAP.end()) {
            if (!sensor_string_.empty()) sensor_string_ += ", ";
            sensor_string_ += it->second;
        }
    }
}

CameraInfo::CameraInfo(const std::string& arch) {
    auto lbracket = arch.rfind('[');
    auto colon = arch.rfind(':');
    auto rbracket = arch.rfind(']');
    if (lbracket == std::string::npos || colon == std::string::npos || rbracket == std::string::npos ||
        colon <= lbracket || rbracket <= colon) {
        throw std::invalid_argument("Invalid arch string format: " + arch);
    }
    board_ = {};
    board_.boardType = arch.substr(lbracket + 1, colon - lbracket - 1);
    std::string archPrefix = arch.substr(0, lbracket);
    while (!archPrefix.empty() && archPrefix.back() == ' ') archPrefix.pop_back();
    for (const auto& b : allBoards()) {
        if (b.archString == archPrefix) {
            board_ = b;
            break;
        }
    }
    std::string idHex = arch.substr(colon + 1, rbracket - colon - 1);
    if (idHex.size() != 24) {
        throw std::invalid_argument("Invalid device ID length in arch string: " + arch);
    }
    for (size_t i = 0; i < 3; i++) {
        device_id_[i] = static_cast<uint32_t>(std::strtoul(idHex.substr(i * 8, 8).c_str(), nullptr, 16));
    }
}

void CameraInfo::setFwVersion(uint32_t major, uint32_t minor, uint32_t patch) {
    fw_version_[0] = major;
    fw_version_[1] = minor;
    fw_version_[2] = patch;
    if (std::make_tuple(major, minor, patch) < std::make_tuple(4U, 5U, 6U)) {
        throw std::runtime_error("Firmware version " + fwVersionString() + " is too old, requires >= 4.5.6");
    }
}

void CameraInfo::setDeviceId(const std::array<uint32_t, 3>& ids) {
    device_id_ = ids;
}

void CameraInfo::setCapabilities(uint32_t cap) {
    capabilities_ = cap;
}

void CameraInfo::setProtocolVersion(uint32_t ver) {
    protocol_version_ = ver;
}

void CameraInfo::checkLicense() {
    try {
        if (!board_.checkLicense) {
            licensed_ = true;
            return;
        }
        auto res = postForm("/check.php", {{"board", board_.boardType}, {"id", deviceIdHex()}});
        const auto& body = res->body;
        if (body.find("<p>Yes</p>") != std::string::npos) {
            licensed_ = true;
            return;
        }
        if (body.find("<p>No</p>") != std::string::npos) {
            licensed_ = false;
            return;
        }
    } catch (...) {
        // Fail-open: don't block users when the license server is unreachable
    }
    licensed_ = true;
}

void CameraInfo::registerLicense(const std::string& boardKey) {
    static const std::regex kKeyPattern(R"(^[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}$)");
    if (!std::regex_match(boardKey, kKeyPattern)) {
        throw std::invalid_argument("Invalid board key format: " + boardKey +
                                    " (expected XXXXX-XXXXX-XXXXX-XXXXX-XXXXX)");
    }

    if (!board_.checkLicense) {
        throw std::runtime_error("Board " + board_.name + " does not require license registration");
    }
    auto res = postForm("/register.php", {{"board", board_.boardType}, {"id", deviceIdHex()}, {"id_key", boardKey}});
    const auto& body = res->body;

    if (body.find("<p>Done</p>") != std::string::npos) {
        licensed_ = true;
        return;
    }
    if (body.find("Board and ID already registered") != std::string::npos) {
        licensed_ = true;
        return;
    }
    if (body.find("Invalid ID Key for board type") != std::string::npos) {
        throw std::runtime_error("Invalid board key for this board type");
    }
    if (body.find("Invalid ID Key") != std::string::npos) {
        throw std::runtime_error("Invalid board key");
    }
    if (body.find("ID Key already used") != std::string::npos) {
        throw std::runtime_error("Board key already used");
    }
    throw std::runtime_error("Unexpected license register response: " + body);
}

std::string CameraInfo::fwVersionString() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d.%d.%d", fw_version_[0], fw_version_[1], fw_version_[2]);
    return buf;
}

std::string CameraInfo::deviceIdHex() const {
    char buf[25];
    std::snprintf(buf, sizeof(buf), "%08X%08X%08X", device_id_[0], device_id_[1], device_id_[2]);
    return buf;
}

}  // namespace mcp
