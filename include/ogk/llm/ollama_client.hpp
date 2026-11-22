#pragma once

#include <ogk/llm/llm_client.hpp>
#include <ogk/llm/llm_errors.hpp>

#include <string>

namespace ogk::llm {

struct OllamaClientConfig {
    std::string base_url = "http://localhost:11434";

    std::string model;

    int connect_timeout_seconds = 10;

    int request_timeout_seconds = 300;
};

class OllamaClient : public LLMClient {
public:
    explicit OllamaClient(const OllamaClientConfig& config)
        : config_(config) {
        if (config_.model.empty()) {
            throw InvalidRequestError("Ollama model must not be empty");
        }
        if (config_.connect_timeout_seconds <= 0) {
            throw InvalidRequestError(
                "Ollama connect_timeout_seconds must be greater than zero"
            );
        }
        if (config_.request_timeout_seconds <= 0) {
            throw InvalidRequestError(
                "Ollama request_timeout_seconds must be greater than zero"
            );
        }
    }

    ChatResponse chat(const ChatRequest& request) override;

private:
    OllamaClientConfig config_;
};

} // namespace ogk::llm
