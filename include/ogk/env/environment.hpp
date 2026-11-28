#pragma once

#include <chrono>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ogk::env {

enum class EnvironmentErrorKind {
    InvalidArgument,
    PermissionDenied,
    NotFound,
    ResourceUnavailable,
    ExecutionFailed
};

class EnvironmentError : public std::exception {
public:
    EnvironmentError(EnvironmentErrorKind kind, std::string message)
        : kind_(kind),
          message_(std::move(message)) {}

    [[nodiscard]] EnvironmentErrorKind kind() const noexcept {
        return kind_;
    }

    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    EnvironmentErrorKind kind_;
    std::string message_;
};

enum class ProcessStopReason {
    Exited,
    TimedOut,
    OutputLimit,
    Signaled
};

struct ProcessRequest {
    std::string executable;
    std::vector<std::string> arguments;
    std::chrono::milliseconds timeout;
    std::size_t stdout_limit_bytes;
    std::size_t stderr_limit_bytes;
};

struct ProcessResult {
    ProcessStopReason stop_reason = ProcessStopReason::Exited;
    std::optional<int> exit_code;
    std::optional<int> signal_number;
    std::vector<std::byte> stdout_bytes;
    std::vector<std::byte> stderr_bytes;
    bool truncated = false;
};

class Environment {
public:
    virtual ~Environment() = default;

    [[nodiscard]] virtual std::string read_text(std::string_view path) = 0;

    virtual void write_text(
        std::string_view path,
        std::string_view content,
        bool overwrite
    ) = 0;

    [[nodiscard]] virtual std::vector<std::byte> read_binary(
        std::string_view path
    ) = 0;

    [[nodiscard]] virtual ProcessResult run_process(
        const ProcessRequest& request
    ) = 0;
};

} // namespace ogk::env
