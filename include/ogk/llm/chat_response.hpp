#pragma once

#include <ogk/llm/message.hpp>

#include <optional>
#include <string>

namespace ogk::llm {

struct Usage {
    std::optional<int> input_tokens;
    std::optional<int> output_tokens;
};

struct ChatResponse {
    Message message;

    std::optional<std::string> finish_reason;

    std::optional<Usage> usage;
};

} // namespace ogk::llm
