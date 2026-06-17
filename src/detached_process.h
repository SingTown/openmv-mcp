#pragma once

#include <string>

namespace mcp {

// Ensure a compatible OpenMV MCP HTTP server is running on the given port.
// Older or unversioned OpenMV MCP servers are stopped before launching the
// current executable in internal_server mode.
void ensureServerRunning(const std::string& executable,
                         int port,
                         const std::string& log_path,
                         const std::string& log_level);

// Stop the OpenMV MCP HTTP server on the given port, if one is running.
// Throws if a running server cannot be stopped.
void shutdownServer(int port);

}  // namespace mcp
