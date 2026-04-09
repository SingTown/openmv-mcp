#ifdef _WIN32

#include <windows.h>

#include "subprocess/subprocess.h"

namespace mcp {

Subprocess::Subprocess(const std::vector<std::string>& /*commands*/,
                       const std::filesystem::path& /*cwd*/,
                       OutputCallback callback,
                       const std::atomic<bool>& cancelled)
    : callback_(std::move(callback)), cancelled_(cancelled) {
    std::string err = "Subprocess not implemented on Windows";
    output_buf_.append(err);
    finished_ = true;
}

void Subprocess::kill() {}

void Subprocess::readLoop(int /*pid*/) {}

}  // namespace mcp

#endif  // _WIN32
