#pragma once

#include <string>

namespace mcp {

// Detach the current process from its controlling terminal.
// POSIX: double-fork daemon — only the final child returns.
// Windows: re-launches the process detached (CreateProcess + DETACHED_PROCESS)
//          and the current (parent) process calls ExitProcess(0) — does not return.
// Throws std::runtime_error on failure.
//
// log_path: if non-empty, stdout/stderr are appended to that file; otherwise
// they are redirected to /dev/null (POSIX) or NUL (Windows).
// stdin is always /dev/null or NUL.
void daemonize(int argc, char* argv[], const std::string& log_path);

}  // namespace mcp
