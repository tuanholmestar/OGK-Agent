#pragma once

#include <ogk/env/environment.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ogk::env::testing {

class FakeEnvironment final : public Environment {
public:
    [[nodiscard]] std::string read_text(std::string_view path) override {
        const auto file = text_files_.find(std::string(path));
        if (file == text_files_.end()) {
            throw EnvironmentError(
                EnvironmentErrorKind::NotFound,
                "Text file not found"
            );
        }

        return file->second;
    }

    void write_text(
        std::string_view path,
        std::string_view content,
        bool overwrite
    ) override {
        const std::string key(path);
        const auto file = text_files_.find(key);
        const auto binary_file = binary_files_.find(key);
        if (
            !overwrite &&
            (file != text_files_.end() || binary_file != binary_files_.end())
        ) {
            throw EnvironmentError(
                EnvironmentErrorKind::ExecutionFailed,
                "Text file already exists"
            );
        }

        binary_files_.erase(key);
        text_files_.insert_or_assign(key, std::string(content));
    }

    [[nodiscard]] std::vector<std::byte> read_binary(
        std::string_view path
    ) override {
        const auto file = binary_files_.find(std::string(path));
        if (file == binary_files_.end()) {
            throw EnvironmentError(
                EnvironmentErrorKind::NotFound,
                "Binary file not found"
            );
        }

        return file->second;
    }

    [[nodiscard]] ProcessResult run_process(
        const ProcessRequest&
    ) override {
        if (process_error_.has_value()) {
            throw EnvironmentError(
                process_error_->kind(),
                process_error_->what()
            );
        }

        if (process_result_.has_value()) {
            return *process_result_;
        }

        throw EnvironmentError(
            EnvironmentErrorKind::ExecutionFailed,
            "No process result was configured"
        );
    }

    void add_text_file(std::string path, std::string content) {
        binary_files_.erase(path);
        text_files_.insert_or_assign(
            std::move(path),
            std::move(content)
        );
    }

    void add_binary_file(
        std::string path,
        std::vector<std::byte> bytes
    ) {
        text_files_.erase(path);
        binary_files_.insert_or_assign(
            std::move(path),
            std::move(bytes)
        );
    }

    void set_process_result(ProcessResult result) {
        process_error_.reset();
        process_result_.emplace(std::move(result));
    }

    void set_process_error(
        EnvironmentErrorKind kind,
        std::string message
    ) {
        process_result_.reset();
        process_error_.emplace(kind, std::move(message));
    }

    [[nodiscard]] const std::string& get_written_file(
        std::string_view path
    ) const {
        const auto file = text_files_.find(std::string(path));
        if (file == text_files_.end()) {
            throw EnvironmentError(
                EnvironmentErrorKind::NotFound,
                "Text file not found"
            );
        }

        return file->second;
    }

private:
    std::unordered_map<std::string, std::string> text_files_;
    std::unordered_map<std::string, std::vector<std::byte>> binary_files_;
    std::optional<ProcessResult> process_result_;
    std::optional<EnvironmentError> process_error_;
};

} // namespace ogk::env::testing
