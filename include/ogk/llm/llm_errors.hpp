#pragma once

#include <stdexcept>
#include <string>

namespace ogk::llm {

class LLMError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class TimeoutKind {
    Connect,
    Request
};

class TimeoutError : public LLMError {
public:
    TimeoutError(TimeoutKind kind, const std::string& message);

    TimeoutKind kind() const noexcept;

private:
    TimeoutKind kind_;
};

class ApiError : public LLMError {
public:
    ApiError(
        int status_code,
        const std::string& message,
        const std::string& response_body
    );

    int status_code() const noexcept;

    const std::string& response_body() const noexcept;

private:
    int status_code_;
    std::string response_body_;
};

class ConnectionError : public LLMError {
public:
    using LLMError::LLMError;
};

class ResponseParseError : public LLMError {
public:
    using LLMError::LLMError;
};

class InvalidRequestError : public LLMError {
public:
    using LLMError::LLMError;
};

} // namespace ogk::llm
