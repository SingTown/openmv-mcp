#pragma once

#include <string>

namespace mcp {

// Check if a board is licensed. Returns true if registered.
// Skips the check (returns true) for boards where Board::checkLicense is false.
bool licenseCheck(const std::string& board_name, const std::string& id);

// Register a board using a Board Key (format: XXXXX-XXXXX-XXXXX-XXXXX-XXXXX).
// Throws on failure (invalid key, network error, etc).
void licenseRegister(const std::string& board_name, const std::string& id, const std::string& board_key);

}  // namespace mcp
