#pragma once

#include <ogk/llm/chat_options.hpp>
#include <ogk/llm/message.hpp>
#include <ogk/llm/tool_definition.hpp>

#include <vector>

namespace ogk::llm {

struct ChatRequest {
    std::vector<Message> messages;

    std::vector<ToolDefinition> tools;

    ChatOptions options;
};

} // namespace ogk::llm
