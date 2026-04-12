#pragma once

#include <filesystem>

namespace mcp {

void extractEmbeddedResource();
const std::filesystem::path& resourcePath();

}  // namespace mcp
