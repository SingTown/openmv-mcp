#include "subprocess/subprocess.h"

namespace mcp {

Subprocess::~Subprocess() {
    kill();
    join();
}

void Subprocess::join() {
    if (thread_.joinable()) {
        thread_.join();
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
