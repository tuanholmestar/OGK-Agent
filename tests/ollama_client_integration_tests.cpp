
#include <ogk/llm/ollama_client.hpp>
#include <ogk/llm/chat_request.hpp>
#include <ogk/llm/chat_response.hpp>
#include <ogk/llm/message.hpp>

#include <nlohmann/json.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>


using namespace ogk::llm;
using json = nlohmann::json;

namespace {

int total_tests = 0;
int failed_tests = 0;


void check(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}


void run_test(
    const std::string& name,
    const std::function<void()>& test
)
{
    ++total_tests;

    std::cout << "[ RUN      ] " << name << '\n';

    try {
        test();
        std::cout << "[       OK ] " << name << '\n';
    }
    catch (const std::exception& error) {
        ++failed_tests;

        std::cout << "[  FAILED  ] " << name << '\n';
        std::cout << "             " << error.what() << '\n';
    }
}


OllamaClientConfig create_config(const std::string& model)
{
    OllamaClientConfig config;

    config.base_url = "http://127.0.0.1:8000";
    config.model = model;
    config.connect_timeout_seconds = 2;
    config.request_timeout_seconds = 5;

    return config;
}


std::string get_response_text(const ChatResponse& response)
{
    for (const auto& part : response.message.content) {
        if (const auto* text = std::get_if<TextContent>(&part)) {
            return text->text;
        }
    }

    return "";
}


// Test 1: Happy path.
void test_happy_path()
{
    OllamaClient client(create_config("mock-model"));

    ChatRequest request;

    request.messages.push_back({
        MessageRole::User,
        {TextContent{"Hello Mock Server"}},
        {},
        std::nullopt,
        std::nullopt
    });

    const ChatResponse response = client.chat(request);

    check(
        response.message.role == MessageRole::Assistant,
        "Response role should be Assistant"
    );

    check(
        response.message.content.size() == 1,
        "Response should contain one content item"
    );

    check(
        get_response_text(response) == "Hello from Mock Ollama Server!",
        "Unexpected response content"
    );

    check(
        response.finish_reason.has_value() &&
        *response.finish_reason == "stop",
        "Finish reason should be stop"
    );

    check(
        response.usage.has_value(),
        "Usage should exist"
    );

    check(
        response.usage->input_tokens.has_value() &&
        *response.usage->input_tokens == 10,
        "Input tokens should be 10"
    );

    check(
        response.usage->output_tokens.has_value() &&
        *response.usage->output_tokens == 8,
        "Output tokens should be 8"
    );
}


// Test 2: Request serialization.
void test_request_serialization()
{
    OllamaClient client(create_config("mock-echo-request"));

    Message user_message{
        MessageRole::User,
        {
            TextContent{"Analyze this image"},
            ImageContent{"dGVzdF9pbWFnZV9iYXNlNjQ="}
        },
        {},
        std::nullopt,
        std::nullopt
    };

    Message assistant_message{
        MessageRole::Assistant,
        {},
        {
            ToolCall{
                std::nullopt,
                "calculator",
                json{{"expression", "1 + 1"}}
            }
        },
        std::nullopt,
        std::nullopt
    };

    ToolDefinition tool{
        "calculator",
        "Calculate mathematical expressions",
        json{
            {"type", "object"},
            {"properties", {
                {"expression", {
                    {"type", "string"}
                }}
            }},
            {"required", {"expression"}}
        }
    };

    ChatRequest request{
        {user_message, assistant_message},
        {tool},
        {}
    };

    request.options.temperature = 0.7;
    request.options.max_tokens = 128;
    request.options.num_ctx = 2048;
    request.options.top_p = 0.9;
    request.options.seed = 42;
    request.options.stop = {"END", "STOP"};

    const ChatResponse response = client.chat(request);

    const std::string response_text = get_response_text(response);

    check(
        !response_text.empty(),
        "Echo response should not be empty"
    );

    const json echoed_request = json::parse(response_text);

    check(
        echoed_request["model"] == "mock-echo-request",
        "Model should be serialized correctly"
    );

    check(
        echoed_request["stream"] == false,
        "Stream should be false"
    );

    check(
        echoed_request["messages"].size() == 2,
        "There should be two serialized messages"
    );

    check(
        echoed_request["messages"][0]["content"] == "Analyze this image",
        "User text content should be serialized correctly"
    );

    check(
        echoed_request["messages"][0]["images"][0] ==
        "dGVzdF9pbWFnZV9iYXNlNjQ=",
        "Image data should be serialized correctly"
    );

    check(
        echoed_request["messages"][1]["tool_calls"][0]
            ["function"]["name"] == "calculator",
        "Tool call name should be serialized correctly"
    );

    check(
        echoed_request["tools"][0]["function"]["name"] == "calculator",
        "Tool definition should be serialized correctly"
    );

    check(
        echoed_request["options"]["temperature"] == 0.7,
        "Temperature should be serialized correctly"
    );

    check(
        echoed_request["options"]["num_predict"] == 128,
        "Max tokens should map to num_predict"
    );

    check(
        echoed_request["options"]["num_ctx"] == 2048,
        "Context size should be serialized correctly"
    );

    check(
        echoed_request["options"]["top_p"] == 0.9,
        "Top P should be serialized correctly"
    );

    check(
        echoed_request["options"]["seed"] == 42,
        "Seed should be serialized correctly"
    );

    check(
        echoed_request["options"]["stop"].size() == 2,
        "Stop sequences should be serialized correctly"
    );
}


// Test 3: HTTP error.
void test_http_error()
{
    OllamaClient client(create_config("mock-http-error"));

    ChatRequest request;

    request.messages.push_back({
        MessageRole::User,
        {TextContent{"Test HTTP error"}},
        {},
        std::nullopt,
        std::nullopt
    });

    bool caught_error = false;

    try {
        client.chat(request);
    }
    catch (const ApiError& error) {
        caught_error = true;

        check(
            error.status_code() == 500,
            "HTTP status code should be 500"
        );
    }

    check(
        caught_error,
        "Should throw ApiError for HTTP 500"
    );
}


// Test 4: Malformed JSON.
void test_malformed_json()
{
    OllamaClient client(create_config("mock-malformed-json"));

    ChatRequest request;

    request.messages.push_back({
        MessageRole::User,
        {TextContent{"Test malformed JSON"}},
        {},
        std::nullopt,
        std::nullopt
    });

    bool caught_error = false;

    try {
        client.chat(request);
    }
    catch (const ResponseParseError&) {
        caught_error = true;
    }

    check(
        caught_error,
        "Should throw ResponseParseError for malformed JSON"
    );
}


// Test 5: Invalid response structure.
void test_invalid_response()
{
    OllamaClient client(create_config("mock-invalid-response"));

    ChatRequest request;

    request.messages.push_back({
        MessageRole::User,
        {TextContent{"Test invalid response"}},
        {},
        std::nullopt,
        std::nullopt
    });

    bool caught_error = false;

    try {
        client.chat(request);
    }
    catch (const ResponseParseError&) {
        caught_error = true;
    }

    check(
        caught_error,
        "Should throw ResponseParseError for invalid response"
    );
}


// Test 6: Connection error.
void test_connection_error()
{
    OllamaClientConfig config;

    config.base_url = "http://127.0.0.1:1";
    config.model = "mock-model";
    config.connect_timeout_seconds = 2;
    config.request_timeout_seconds = 5;

    OllamaClient client(config);

    ChatRequest request;

    request.messages.push_back({
        MessageRole::User,
        {TextContent{"Test connection error"}},
        {},
        std::nullopt,
        std::nullopt
    });

    bool caught_error = false;

    try {
        client.chat(request);
    }
    catch (const ConnectionError&) {
        caught_error = true;
    }

    check(
        caught_error,
        "Should throw ConnectionError when connection fails"
    );
}


// Test 7: Request timeout.
void test_request_timeout()
{
    auto config = create_config("mock-slow-response");

    config.connect_timeout_seconds = 2;
    config.request_timeout_seconds = 1;

    OllamaClient client(config);

    ChatRequest request;

    request.messages.push_back({
        MessageRole::User,
        {TextContent{"Test request timeout"}},
        {},
        std::nullopt,
        std::nullopt
    });

    bool caught_timeout = false;

    try {
        client.chat(request);
    }
    catch (const TimeoutError& error) {
        caught_timeout = true;

        check(
            error.kind() == TimeoutKind::Request,
            "Timeout should be classified as Request"
        );
    }

    check(
        caught_timeout,
        "Should throw TimeoutError when server responds too slowly"
    );
}

} // namespace


int main()
{
    run_test(
        "OllamaClient HTTP request and response",
        test_happy_path
    );

    run_test(
        "OllamaClient request serialization",
        test_request_serialization
    );

    run_test(
        "OllamaClient HTTP error",
        test_http_error
    );

    run_test(
        "OllamaClient malformed JSON",
        test_malformed_json
    );

    run_test(
        "OllamaClient invalid response",
        test_invalid_response
    );

    run_test(
        "OllamaClient connection error",
        test_connection_error
    );

    run_test(
        "OllamaClient request timeout",
        test_request_timeout
    );

    std::cout << "\n========== TEST SUMMARY ==========\n";
    std::cout << "Total tests: " << total_tests << '\n';
    std::cout << "Passed:      " << total_tests - failed_tests << '\n';
    std::cout << "Failed:      " << failed_tests << '\n';
    std::cout << "==================================\n";

    return failed_tests == 0 ? 0 : 1;
}

