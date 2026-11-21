#pragma once

#include <ogk/llm/tool_call.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ogk::llm {

enum class MessageRole {
    System,
    User,
    Assistant,
    Tool
};

struct TextContent {
    std::string text;
};

struct ImageContent {
    std::string base64_data;
};

using ContentPart = std::variant<TextContent, ImageContent>;

struct Message {
    MessageRole role;

    std::vector<ContentPart> content;

    std::vector<ToolCall> tool_calls;

    std::optional<std::string> tool_name;

    std::optional<std::string> tool_call_id;
};

} // namespace ogk::llm
