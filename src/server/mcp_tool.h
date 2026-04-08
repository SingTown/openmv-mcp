#pragma once

#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "server/mcp_context.h"

namespace mcp {

class McpContent {
 public:
    void addText(const json& value) { content_.push_back({{"type", "text"}, {"text", value.dump()}}); }

    void addImage(const std::string& base64_data, const std::string& mime_type) {
        content_.push_back({{"type", "image"}, {"data", base64_data}, {"mimeType", mime_type}});
    }

    [[nodiscard]] json toContent() const { return content_; }

 private:
    json content_ = json::array();
};

using ToolHandler = std::function<McpContent(McpContext&, const json&, const std::atomic<bool>& cancelled)>;

struct McpTool {
    std::string name;
    std::string description;
    json input_schema;
    ToolHandler handler;
    bool streaming = false;
};

extern const std::vector<const McpTool*> ALL_MCP_TOOLS;

}  // namespace mcp
