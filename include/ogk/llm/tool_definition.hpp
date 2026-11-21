#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace ogk::llm {

struct ToolDefinition {
    std::string name;
    std::string description;
    nlohmann::json parameters;
};

} // namespace ogk::llm
