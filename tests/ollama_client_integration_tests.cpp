

#include <ogk/llm/ollama_client.hpp>
#include <ogk/llm/chat_request.hpp>
#include <ogk/llm/chat_response.hpp>
#include <ogk/llm/message.hpp>

#include <nlohmann/json.hpp>

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

using namespace ogk::llm;
using namespace std;

using json = nlohmann::json;

// Kiểm tra một điều kiện.
void check(bool condition, const string& message)
{
    if (!condition) {
        throw runtime_error(message);
    }
}

// Lấy nội dung TextContent từ response.
string get_response_text(const ChatResponse& response)
{
    check(
        response.message.content.size() == 1,
        "Incorrect response content size"
    );

    const auto* text =
        get_if<TextContent>(&response.message.content[0]);

    check(
        text != nullptr,
        "Response content is not TextContent"
    );

    return text->text;
}

// Chạy từng test và ghi nhận kết quả.
void run_test(
    const string& name,
    const function<void()>& test,
    int& passed,
    int& failed
)
{
    try {
        test();

        cout << "[PASS] " << name << '\n';
        ++passed;
    }
    catch (const exception& error) {
        cout << "[FAIL] " << name
             << " - " << error.what() << '\n';

        ++failed;
    }
}

// Tạo client kết nối Mock Server.
OllamaClientConfig create_config(const string& model)
{
    OllamaClientConfig config;

    config.base_url = "http://127.0.0.1:8000";
    config.model = model;

    return config;
}

int main()
{
    int passed = 0;
    int failed = 0;

    // TEST 1: HTTP request và response thành công.
    run_test(
        "OllamaClient HTTP request and response",
        []() {
            OllamaClient client(create_config("mock-model"));

            Message user_message{
                MessageRole::User,
                {TextContent{"Hello Mock Server"}},
                {},
                nullopt,
                nullopt
            };

            ChatRequest request{
                {user_message},
                {},
                {}
            };

            ChatResponse response = client.chat(request);

            check(
                response.message.role == MessageRole::Assistant,
                "Incorrect response role"
            );

            check(
                get_response_text(response) ==
                    "Hello from Mock Ollama Server!",
                "Incorrect response content"
            );

            check(
                response.finish_reason.has_value(),
                "Missing finish reason"
            );

            check(
                response.finish_reason.value() == "stop",
                "Incorrect finish reason"
            );

            check(
                response.usage.has_value(),
                "Missing usage information"
            );

            check(
                response.usage->input_tokens == 10,
                "Incorrect input token count"
            );

            check(
                response.usage->output_tokens == 8,
                "Incorrect output token count"
            );
        },
        passed,
        failed
    );

    // TEST 2: Kiểm tra request serialization.
    run_test(
        "OllamaClient request serialization",
        []() {
            OllamaClient client(
                create_config("mock-echo-request")
            );

            // User message có text và image.
            Message user_message{
                MessageRole::User,
                {
                    TextContent{"Analyze this image"},
                    ImageContent{"dGVzdF9pbWFnZV9iYXNlNjQ="}
                },
                {},
                nullopt,
                nullopt
            };

            // Assistant message có tool call.
            Message assistant_message{
                MessageRole::Assistant,
                {},
                {
                    ToolCall{
                        nullopt,
                        "calculator",
                        json{{"expression", "1 + 1"}}
                    }
                },
                nullopt,
                nullopt
            };

            // Tool definition.
            ToolDefinition tool{
                "calculator",
                "Calculate a mathematical expression",
                json{
                    {"type", "object"},
                    {"properties", {
                        {"expression", {
                            {"type", "string"}
                        }}
                    }}
                }
            };

            ChatRequest request{
                {user_message, assistant_message},
                {tool},
                {}
            };

            // Cấu hình options.
            request.options.temperature = 0.7;
            request.options.max_tokens = 128;
            request.options.num_ctx = 2048;
            request.options.top_p = 0.9;
            request.options.seed = 42;
            request.options.stop = {"END", "STOP"};

            ChatResponse response = client.chat(request);

            // Mock Server trả lại request dưới dạng JSON string.
            const string response_text = get_response_text(response);
            const json body = json::parse(response_text);

            // Kiểm tra model và stream.
            check(
                body["model"] == "mock-echo-request",
                "Incorrect serialized model"
            );

            check(
                body["stream"] == false,
                "Stream should be disabled"
            );

            // Kiểm tra messages.
            check(
                body["messages"].size() == 2,
                "Incorrect serialized message count"
            );

            check(
                body["messages"][0]["role"] == "user",
                "Incorrect user role"
            );

            check(
                body["messages"][0]["content"] ==
                    "Analyze this image",
                "Incorrect serialized text content"
            );

            check(
                body["messages"][0]["images"].size() == 1,
                "Incorrect serialized image count"
            );

            check(
                body["messages"][0]["images"][0] ==
                    "dGVzdF9pbWFnZV9iYXNlNjQ=",
                "Incorrect serialized image data"
            );

            // Kiểm tra assistant tool call.
            check(
                body["messages"][1]["tool_calls"].size() == 1,
                "Incorrect serialized tool call count"
            );

            check(
                body["messages"][1]["tool_calls"][0]
                    ["function"]["name"] == "calculator",
                "Incorrect serialized tool call name"
            );

            check(
                body["messages"][1]["tool_calls"][0]
                    ["function"]["arguments"]["expression"] == "1 + 1",
                "Incorrect serialized tool call arguments"
            );

            // Kiểm tra tool definition.
            check(
                body["tools"].size() == 1,
                "Incorrect serialized tool count"
            );

            check(
                body["tools"][0]["function"]["name"] == "calculator",
                "Incorrect serialized tool name"
            );

            check(
                body["tools"][0]["function"]["description"] ==
                    "Calculate a mathematical expression",
                "Incorrect serialized tool description"
            );

            // Kiểm tra options.
            check(
                body["options"]["temperature"] == 0.7,
                "Incorrect temperature"
            );

            check(
                body["options"]["num_predict"] == 128,
                "Incorrect max_tokens serialization"
            );

            check(
                body["options"]["num_ctx"] == 2048,
                "Incorrect num_ctx"
            );

            check(
                body["options"]["top_p"] == 0.9,
                "Incorrect top_p"
            );

            check(
                body["options"]["seed"] == 42,
                "Incorrect seed"
            );

            check(
                body["options"]["stop"].size() == 2,
                "Incorrect stop sequence count"
            );
        },
        passed,
        failed
    );

    // TEST 3: HTTP 500 phải tạo ApiError.
    run_test(
        "OllamaClient HTTP error handling",
        []() {
            OllamaClient client(
                create_config("mock-http-error")
            );

            Message user_message{
                MessageRole::User,
                {TextContent{"Trigger HTTP error"}},
                {},
                nullopt,
                nullopt
            };

            ChatRequest request{
                {user_message},
                {},
                {}
            };

            bool caught = false;

            try {
                client.chat(request);
            }
            catch (const ApiError&) {
                caught = true;
            }

            check(
                caught,
                "Expected ApiError was not thrown"
            );
        },
        passed,
        failed
    );

    // TEST 4: Malformed JSON phải tạo ResponseParseError.
    run_test(
        "OllamaClient malformed JSON handling",
        []() {
            OllamaClient client(
                create_config("mock-malformed-json")
            );

            Message user_message{
                MessageRole::User,
                {TextContent{"Trigger malformed JSON"}},
                {},
                nullopt,
                nullopt
            };

            ChatRequest request{
                {user_message},
                {},
                {}
            };

            bool caught = false;

            try {
                client.chat(request);
            }
            catch (const ResponseParseError&) {
                caught = true;
            }

            check(
                caught,
                "Expected ResponseParseError was not thrown"
            );
        },
        passed,
        failed
    );

    // TEST 5: JSON hợp lệ nhưng thiếu message.
    run_test(
        "OllamaClient invalid response structure",
        []() {
            OllamaClient client(
                create_config("mock-invalid-response")
            );

            Message user_message{
                MessageRole::User,
                {TextContent{"Trigger invalid response"}},
                {},
                nullopt,
                nullopt
            };

            ChatRequest request{
                {user_message},
                {},
                {}
            };

            bool caught = false;

            try {
                client.chat(request);
            }
            catch (const ResponseParseError&) {
                caught = true;
            }

            check(
                caught,
                "Expected ResponseParseError was not thrown"
            );
        },
        passed,
        failed
    );

    // TEST 6: Connection refused phải tạo ConnectionError.
    run_test(
        "OllamaClient connection error handling",
        []() {
            OllamaClientConfig config;
            config.base_url = "http://127.0.0.1:1";
            config.model = "mock-model";
            config.connect_timeout_seconds = 2;
            config.request_timeout_seconds = 5;

            OllamaClient client(config);

            Message user_message{
                MessageRole::User,
                {TextContent{"Trigger connection error"}},
                {},
                nullopt,
                nullopt
            };

            ChatRequest request{
                {user_message},
                {},
                {}
            };

            bool caught = false;

            try {
                client.chat(request);
            }
            catch (const ConnectionError&) {
                caught = true;
            }

            check(
                caught,
                "Expected ConnectionError was not thrown"
            );
        },
        passed,
        failed
    );

    // Tổng kết.
    cout << "\n========== INTEGRATION TEST SUMMARY ==========\n";
    cout << "Passed: " << passed << '\n';
    cout << "Failed: " << failed << '\n';
    cout << "Total : " << passed + failed << '\n';

    return failed == 0 ? 0 : 1;
}
