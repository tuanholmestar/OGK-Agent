
#include <ogk/llm/llm_errors.hpp>

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

    // Test 1: LLMError
    run_test("LLMError message", []()
    {
        LLMError error("Test error");

        check(
            string(error.what()) == "Test error",
            "Incorrect error message"
        );
    }, passed, failed);

    // Test 2: TimeoutError - Connect
    run_test("TimeoutError Connect", []()
    {
        TimeoutError error(
            TimeoutKind::Connect,
            "Connection timeout"
        );

        check(
            error.kind() == TimeoutKind::Connect,
            "Incorrect timeout kind"
        );

        check(
            string(error.what()) == "Connection timeout",
            "Incorrect error message"
        );
    }, passed, failed);

    // Test 3: TimeoutError - Request
    run_test("TimeoutError Request", []()
    {
        TimeoutError error(
            TimeoutKind::Request,
            "Request timeout"
        );

        check(
            error.kind() == TimeoutKind::Request,
            "Incorrect timeout kind"
        );
    }, passed, failed);

    // Test 4: ApiError basic
    run_test("ApiError basic", []()
    {
        ApiError error(
            500,
            "Internal Server Error",
            "Server failed"
        );

        check(
            error.status_code() == 500,
            "Incorrect status code"
        );

        check(
            error.response_body() == "Server failed",
            "Incorrect response body"
        );

        check(
            string(error.what()) == "Internal Server Error",
            "Incorrect error message"
        );
    }, passed, failed);

    // Test 5: ApiError body exactly 4096 bytes
    run_test("ApiError body exactly 4096 bytes", []()
    {
        const string body(4096, 'A');

        ApiError error(500, "Error", body);

        check(
            error.response_body().size() == 4096,
            "Body should remain unchanged"
        );

        check(
            error.response_body() == body,
            "Body content was modified"
        );
    }, passed, failed);

    // Test 6: ApiError body exceeds 4096 bytes
    run_test("ApiError body exceeds 4096 bytes", []()
    {
        const string body(5000, 'A');

        ApiError error(500, "Error", body);

        const string& result = error.response_body();

        check(
            result.size() == 4096,
            "Body should be truncated to 4096 bytes"
        );

        check(
            result.substr(4085) == "[truncated]",
            "Missing truncation marker"
        );

        check(
            result.substr(0, 4085) == string(4085, 'A'),
            "Incorrect body prefix"
        );
    }, passed, failed);

    // Test 7: ConnectionError
    run_test("ConnectionError", []()
    {
        ConnectionError error("Connection failed");

        check(
            string(error.what()) == "Connection failed",
            "Incorrect error message"
        );
    }, passed, failed);

    // Test 8: ResponseParseError
    run_test("ResponseParseError", []()
    {
        ResponseParseError error("Invalid JSON");

        check(
            string(error.what()) == "Invalid JSON",
            "Incorrect error message"
        );
    }, passed, failed);

    // Test 9: InvalidRequestError
    run_test("InvalidRequestError", []()
    {
        InvalidRequestError error("Empty request");

        check(
            string(error.what()) == "Empty request",
            "Incorrect error message"
        );
    }, passed, failed);

    cout << "\n========== TEST SUMMARY ==========\n";
    cout << "Passed: " << passed << '\n';
    cout << "Failed: " << failed << '\n';
    cout << "Total : " << passed + failed << '\n';

    return failed == 0 ? 0 : 1;
}
