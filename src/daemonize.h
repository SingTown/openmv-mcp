#pragma once

#include <string>

namespace mcp {

// Detach the current process from its controlling terminal (POSIX double-fork).
// On success only the final daemon child returns; the caller's parent and
// intermediate child call _exit(0). Throws std::runtime_error on failure.
// On Windows this function throws — daemonization is not supported there.
//
// log_path: if non-empty, stdout/stderr are appended to that file; otherwise
// they are redirected to /dev/null. stdin is always /dev/null.
void daemonize(const std::string& log_path);

}  // namespace mcp
