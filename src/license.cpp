#include "license.h"

#include <httplib/httplib.h>

#include <regex>
#include <stdexcept>

#include "board.h"

namespace mcp {

static constexpr const char* kHost = "upload.openmv.io";
static constexpr int kTimeoutSec = 3;

static httplib::Result postForm(const std::string& path, const httplib::Params& params) {
    httplib::SSLClient cli(kHost);
    cli.set_connection_timeout(kTimeoutSec);
    cli.set_read_timeout(kTimeoutSec);
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

bool licenseCheck(const std::string& board_name, const std::string& id) {
    auto board = findBoardByName(board_name);
    if (!board.checkLicense) {
        return true;
    }
    try {
        auto res = postForm("/check.php", {{"board", board.boardType}, {"id", id}});
        const auto& body = res->body;
        if (body.find("<p>Yes</p>") != std::string::npos) {
            return true;
        }
        if (body.find("<p>No</p>") != std::string::npos) {
            return false;
        }
    } catch (...) {
        // Fail-open: don't block users when the license server is unreachable
        return true;
    }
    return true;
}

void licenseRegister(const std::string& board_name, const std::string& id, const std::string& board_key) {
    static const std::regex kKeyPattern(R"(^[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}$)");
    if (!std::regex_match(board_key, kKeyPattern)) {
        throw std::invalid_argument("Invalid board key format: " + board_key +
                                    " (expected XXXXX-XXXXX-XXXXX-XXXXX-XXXXX)");
    }

    auto board = findBoardByName(board_name);
    if (!board.checkLicense) {
        throw std::runtime_error("Board " + board_name + " does not require license registration");
    }
    auto res = postForm("/register.php", {{"board", board.boardType}, {"id", id}, {"id_key", board_key}});
    const auto& body = res->body;

    if (body.find("<p>Done</p>") != std::string::npos) {
        return;
    }
    if (body.find("Board and ID already registered") != std::string::npos) {
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

}  // namespace mcp
