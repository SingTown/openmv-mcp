#pragma once

#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "server/mcp_context.h"

namespace mcp {

using json = nlohmann::json;

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

using ToolHandler = std::function<McpContent(McpContext&, const json&)>;

struct McpTool {
    std::string name;
    std::string description;
    json input_schema;
    ToolHandler handler;
};

extern const std::map<std::string, const McpTool*> ALL_MCP_TOOLS;

}  // namespace mcp
