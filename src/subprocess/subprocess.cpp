#include "subprocess/subprocess.h"

#include <stdexcept>

namespace mcp {

Subprocess::~Subprocess() {
    kill();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Subprocess::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
    if (joined_) {
        return;
    }
    joined_ = true;
    if (cancelled_.load()) {
        throw std::runtime_error("Operation cancelled");
    }
    if (exit_code_ != 0) {
        throw std::runtime_error("Command failed with exit code " + std::to_string(exit_code_.load()));
    }
}

std::string Subprocess::readOutput() {
    return output_buf_.take();
}

bool Subprocess::finished() const {
    return finished_;
}

int Subprocess::exitCode() const {
    return exit_code_;
}

}  // namespace mcp
