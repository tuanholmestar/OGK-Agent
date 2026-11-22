#pragma once

#include <ogk/llm/chat_request.hpp>
#include <ogk/llm/chat_response.hpp>

namespace ogk::llm {

class LLMClient {
public:
    virtual ~LLMClient() = default;

    virtual ChatResponse chat(
        const ChatRequest& request
    ) = 0;
};

} // namespace ogk::llm