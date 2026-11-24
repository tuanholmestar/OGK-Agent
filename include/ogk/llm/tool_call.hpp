#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace ogk::llm {

struct ToolCall {
    std::optional<std::string> id;
    std::string name;
    nlohmann::json arguments;
};

} // namespace ogk::llm
