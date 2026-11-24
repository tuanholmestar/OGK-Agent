#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ogk::llm {

struct ChatOptions {
    std::optional<double> temperature;
    std::optional<int> max_tokens;

    std::optional<int> num_ctx;
    std::optional<double> top_p;
    std::optional<int> seed;
    std::vector<std::string> stop;
};

} // namespace ogk::llm
