#include <ogk/llm/llm_errors.hpp>
#include <cstddef>
#include <string>
#include <string_view>

namespace ogk::llm {

TimeoutError::TimeoutError(TimeoutKind kind, const std::string& message)
    : LLMError(message),
      kind_(kind) {}

TimeoutKind TimeoutError::kind() const noexcept {
    return kind_;
}

ApiError::ApiError(
    int status_code,
    const std::string& message,
    const std::string& response_body
) : LLMError(message),
    status_code_(status_code),
    response_body_(response_body) {
    constexpr std::size_t max_response_body_size = 4096;
    constexpr std::string_view truncation_marker = "[truncated]";

    if (response_body_.size() > max_response_body_size) {
        const std::size_t prefix_size =
            max_response_body_size - truncation_marker.size();
        response_body_.resize(prefix_size);
        response_body_.append(truncation_marker);
    }
}

int ApiError::status_code() const noexcept {
    return status_code_;
}

const std::string& ApiError::response_body() const noexcept {
    return response_body_;
}

} // namespace ogk::llm
