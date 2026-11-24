
#include <ogk/llm/ollama_client.hpp>
#include <ogk/llm/chat_request.hpp>
#include <ogk/llm/chat_response.hpp>
#include <ogk/llm/message.hpp>

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

using namespace ogk::llm;
using namespace std;

// Kiểm tra một điều kiện
void check(bool condition, const string& message)
{
    if (!condition)
    {
        throw runtime_error(message);
    }
}

// Chạy từng test và ghi nhận kết quả
void run_test(
    const string& name,
    const function<void()>& test,
    int& passed,
    int& failed
)
{
    try
    {
        test();
        cout << "[PASS] " << name << '\n';
        ++passed;
    }
    catch (const exception& error)
    {
        cout << "[FAIL] " << name
             << " - " << error.what() << '\n';
        ++failed;
    }
}

int main()
{
    int passed = 0;
    int failed = 0;

    run_test("OllamaClient HTTP request and response", []()
    {
        // 1. Cấu hình OllamaClient kết nối Mock Server
        OllamaClientConfig config;
        config.base_url = "http://127.0.0.1:8000";
        config.model = "mock-model";

        OllamaClient client(config);

        // 2. Tạo user message
        Message user_message{
            MessageRole::User,
            {TextContent{"Hello Mock Server"}},
            {},
            nullopt,
            nullopt
        };

        // 3. Tạo ChatRequest
        ChatRequest request{
            {user_message},
            {},
            {}
        };

        // 4. Gửi HTTP request
        ChatResponse response = client.chat(request);

        // 5. Kiểm tra role
        check(
            response.message.role == MessageRole::Assistant,
            "Incorrect response role"
        );

        // 6. Kiểm tra nội dung message
        check(
            response.message.content.size() == 1,
            "Incorrect content size"
        );

        const auto* text_content =
            get_if<TextContent>(&response.message.content[0]);

        check(
            text_content != nullptr,
            "Response content is not TextContent"
        );

        check(
            text_content->text == "Hello from Mock Ollama Server!",
            "Incorrect response content"
        );

        // 7. Kiểm tra finish reason
        check(
            response.finish_reason.has_value(),
            "Missing finish reason"
        );

        check(
            response.finish_reason.value() == "stop",
            "Incorrect finish reason"
        );

        // 8. Kiểm tra token usage
        check(
            response.usage.has_value(),
            "Missing usage information"
        );
        check(
            response.usage->input_tokens.has_value(),
            "Missing input token count"
        );
        check(
            response.usage->input_tokens == 10,
            "Incorrect input token count"
        );
        check(
            response.usage->output_tokens.has_value(),
            "Missing output token count"
        );
        check(
            response.usage->output_tokens == 8,
            "Incorrect output token count"
        );

    }, passed, failed);

    cout << "\n========== INTEGRATION TEST SUMMARY ==========\n";
    cout << "Passed: " << passed << '\n';
    cout << "Failed: " << failed << '\n';
    cout << "Total : " << passed + failed << '\n';

    return failed == 0 ? 0 : 1;
}
