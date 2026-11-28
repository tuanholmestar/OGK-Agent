
#include "support/fake_environment.hpp"

#include <ogk/llm/llm_errors.hpp>
#include <ogk/llm/ollama_client.hpp>

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ogk::llm;
using namespace ogk::env;
using namespace ogk::env::testing;
using namespace std;


// Kiểm tra một điều kiện.
void check(bool condition, const string& message)
{
    if (!condition)
    {
        throw runtime_error(message);
    }
}


// Kiểm tra một đoạn code có ném InvalidRequestError hay không.
void check_invalid_config(
    const function<void()>& action,
    const string& message
)
{
    bool caught = false;

    try
    {
        action();
    }
    catch (const InvalidRequestError&)
    {
        caught = true;
    }

    check(caught, message);
}


void check_environment_error(
    const function<void()>& action,
    EnvironmentErrorKind expected_kind,
    const string& message
)
{
    bool caught = false;

    try
    {
        action();
    }
    catch (const EnvironmentError& error)
    {
        caught = error.kind() == expected_kind;
    }

    check(caught, message);
}


// Chạy từng test và ghi nhận kết quả.
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

    // Test 1: LLMError.
    run_test("LLMError message", []()
    {
        LLMError error("Test error");

        check(
            string(error.what()) == "Test error",
            "Incorrect error message"
        );
    }, passed, failed);


    // Test 2: TimeoutError - Connect.
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


    // Test 3: TimeoutError - Request.
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


    // Test 4: ApiError basic.
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


    // Test 5: ApiError body exactly 4096 bytes.
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


    // Test 6: ApiError body exceeds 4096 bytes.
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


    // Test 7: ConnectionError.
    run_test("ConnectionError", []()
    {
        ConnectionError error("Connection failed");

        check(
            string(error.what()) == "Connection failed",
            "Incorrect error message"
        );
    }, passed, failed);


    // Test 8: ResponseParseError.
    run_test("ResponseParseError", []()
    {
        ResponseParseError error("Invalid JSON");

        check(
            string(error.what()) == "Invalid JSON",
            "Incorrect error message"
        );
    }, passed, failed);


    // Test 9: InvalidRequestError.
    run_test("InvalidRequestError", []()
    {
        InvalidRequestError error("Empty request");

        check(
            string(error.what()) == "Empty request",
            "Incorrect error message"
        );
    }, passed, failed);


    // Test 10: OllamaClient valid configuration.
    run_test("OllamaClient valid config", []()
    {
        OllamaClientConfig config;

        config.base_url = "http://127.0.0.1:8000";
        config.model = "mock-model";
        config.connect_timeout_seconds = 10;
        config.request_timeout_seconds = 30;

        OllamaClient client(config);
    }, passed, failed);


    // Test 11: Empty model.
    run_test("OllamaClient empty model", []()
    {
        OllamaClientConfig config;

        config.model = "";

        check_invalid_config(
            [&]()
            {
                OllamaClient client(config);
            },
            "Empty model should throw InvalidRequestError"
        );
    }, passed, failed);


    // Test 12: Invalid base URL.
    run_test("OllamaClient invalid base URL", []()
    {
        OllamaClientConfig config;

        config.model = "mock-model";
        config.base_url = "ftp://127.0.0.1:8000";

        check_invalid_config(
            [&]()
            {
                OllamaClient client(config);
            },
            "Invalid base URL should throw InvalidRequestError"
        );
    }, passed, failed);


    // Test 13: Invalid connect timeout.
    run_test("OllamaClient invalid connect timeout", []()
    {
        OllamaClientConfig config;

        config.model = "mock-model";
        config.connect_timeout_seconds = 0;

        check_invalid_config(
            [&]()
            {
                OllamaClient client(config);
            },
            "Invalid connect timeout should throw InvalidRequestError"
        );
    }, passed, failed);


    // Test 14: Invalid request timeout.
    run_test("OllamaClient invalid request timeout", []()
    {
        OllamaClientConfig config;

        config.model = "mock-model";
        config.request_timeout_seconds = -1;

        check_invalid_config(
            [&]()
            {
                OllamaClient client(config);
            },
            "Invalid request timeout should throw InvalidRequestError"
        );
    }, passed, failed);


    // Test 15: FakeEnvironment reads an existing text file.
    run_test("FakeEnvironment reads text file", []()
    {
        FakeEnvironment environment;
        environment.add_text_file("notes.txt", "hello");

        check(
            environment.read_text("notes.txt") == "hello",
            "Text file content should match"
        );
    }, passed, failed);


    // Test 16: FakeEnvironment reports a missing text file.
    run_test("FakeEnvironment missing text file", []()
    {
        FakeEnvironment environment;

        check_environment_error(
            [&]()
            {
                (void)environment.read_text("missing.txt");
            },
            EnvironmentErrorKind::NotFound,
            "Missing text file should report NotFound"
        );
    }, passed, failed);


    // Test 17: FakeEnvironment writes a new text file.
    run_test("FakeEnvironment writes new text file", []()
    {
        FakeEnvironment environment;
        environment.write_text("new.txt", "new content", false);

        check(
            environment.get_written_file("new.txt") == "new content",
            "Written text should be stored"
        );
    }, passed, failed);


    // Test 18: FakeEnvironment rejects a disallowed overwrite.
    run_test("FakeEnvironment rejects overwrite", []()
    {
        FakeEnvironment environment;
        environment.add_text_file("existing.txt", "original");

        check_environment_error(
            [&]()
            {
                environment.write_text("existing.txt", "changed", false);
            },
            EnvironmentErrorKind::ExecutionFailed,
            "overwrite=false should report ExecutionFailed"
        );

        check(
            environment.get_written_file("existing.txt") == "original",
            "Rejected overwrite should preserve the original content"
        );
    }, passed, failed);


    // Test 19: FakeEnvironment overwrites an existing text file.
    run_test("FakeEnvironment overwrites text file", []()
    {
        FakeEnvironment environment;
        environment.add_text_file("existing.txt", "original");
        environment.write_text("existing.txt", "replacement", true);

        check(
            environment.get_written_file("existing.txt") == "replacement",
            "overwrite=true should replace existing content"
        );
    }, passed, failed);


    // Test 20: FakeEnvironment reads an existing binary file.
    run_test("FakeEnvironment reads binary file", []()
    {
        FakeEnvironment environment;
        const vector<byte> expected{byte{0x00}, byte{0x41}, byte{0xFF}};
        environment.add_binary_file("image.bin", expected);

        check(
            environment.read_binary("image.bin") == expected,
            "Binary file bytes should match"
        );
    }, passed, failed);


    // Test 21: FakeEnvironment rejects a text write over binary data.
    run_test("FakeEnvironment rejects text write over binary file", []()
    {
        FakeEnvironment environment;
        const vector<byte> original{byte{0x41}};
        environment.add_binary_file("shared.dat", original);

        check_environment_error(
            [&]()
            {
                environment.write_text("shared.dat", "text", false);
            },
            EnvironmentErrorKind::ExecutionFailed,
            "overwrite=false should reject an existing binary path"
        );

        check(
            environment.read_binary("shared.dat") == original,
            "Rejected write should preserve the binary file"
        );
    }, passed, failed);


    // Test 22: FakeEnvironment replaces binary data with a text write.
    run_test("FakeEnvironment overwrites binary file with text", []()
    {
        FakeEnvironment environment;
        environment.add_binary_file("shared.dat", {byte{0x41}});
        environment.write_text("shared.dat", "text", true);

        check(
            environment.get_written_file("shared.dat") == "text",
            "Text content should replace binary data"
        );

        check_environment_error(
            [&]()
            {
                (void)environment.read_binary("shared.dat");
            },
            EnvironmentErrorKind::NotFound,
            "Overwritten binary entry should be removed"
        );
    }, passed, failed);


    // Test 23: FakeEnvironment keeps text and binary paths mutually exclusive.
    run_test("FakeEnvironment shares text and binary path namespace", []()
    {
        FakeEnvironment environment;
        const vector<byte> binary_content{byte{0x42}};
        environment.add_text_file("shared.dat", "text");
        environment.add_binary_file("shared.dat", binary_content);

        check(
            environment.read_binary("shared.dat") == binary_content,
            "Adding binary data should replace text at the same path"
        );

        check_environment_error(
            [&]()
            {
                (void)environment.read_text("shared.dat");
            },
            EnvironmentErrorKind::NotFound,
            "Adding binary data should remove the text entry"
        );

        environment.add_text_file("shared.dat", "replacement");

        check(
            environment.read_text("shared.dat") == "replacement",
            "Adding text should replace binary data at the same path"
        );

        check_environment_error(
            [&]()
            {
                (void)environment.read_binary("shared.dat");
            },
            EnvironmentErrorKind::NotFound,
            "Adding text should remove the binary entry"
        );
    }, passed, failed);


    // Test 24: FakeEnvironment reports a missing binary file.
    run_test("FakeEnvironment missing binary file", []()
    {
        FakeEnvironment environment;

        check_environment_error(
            [&]()
            {
                (void)environment.read_binary("missing.bin");
            },
            EnvironmentErrorKind::NotFound,
            "Missing binary file should report NotFound"
        );
    }, passed, failed);


    // Test 25: FakeEnvironment returns a configured process result.
    run_test("FakeEnvironment configured process result", []()
    {
        FakeEnvironment environment;
        const ProcessResult expected{
            ProcessStopReason::Signaled,
            nullopt,
            9,
            {byte{0x41}},
            {byte{0x42}},
            true
        };
        environment.set_process_result(expected);

        const ProcessResult actual = environment.run_process({
            "ignored-executable",
            {},
            chrono::milliseconds(100),
            128,
            128
        });

        check(
            actual.stop_reason == expected.stop_reason &&
                actual.exit_code == expected.exit_code &&
                actual.signal_number == expected.signal_number &&
                actual.stdout_bytes == expected.stdout_bytes &&
                actual.stderr_bytes == expected.stderr_bytes &&
                actual.truncated == expected.truncated,
            "Configured process result should be returned unchanged"
        );
    }, passed, failed);


    // Test 26: FakeEnvironment throws a configured process error.
    run_test("FakeEnvironment configured process error", []()
    {
        FakeEnvironment environment;
        environment.set_process_error(
            EnvironmentErrorKind::ResourceUnavailable,
            "Configured fake process error"
        );

        check_environment_error(
            [&]()
            {
                (void)environment.run_process({
                    "ignored-executable",
                    {},
                    chrono::milliseconds(100),
                    128,
                    128
                });
            },
            EnvironmentErrorKind::ResourceUnavailable,
            "Configured process error kind should be preserved"
        );
    }, passed, failed);


    // Test 27: FakeEnvironment requires a configured process outcome.
    run_test("FakeEnvironment requires process configuration", []()
    {
        FakeEnvironment environment;
        check_environment_error(
            [&]()
            {
                (void)environment.run_process({
                    "not-a-real-executable",
                    {},
                    chrono::milliseconds(100),
                    128,
                    128
                });
            },
            EnvironmentErrorKind::ExecutionFailed,
            "Unconfigured process should report ExecutionFailed"
        );
    }, passed, failed);


    cout << "\n========== TEST SUMMARY ==========\n";
    cout << "Passed: " << passed << '\n';
    cout << "Failed: " << failed << '\n';
    cout << "Total : " << passed + failed << '\n';
    cout << "==================================\n";

    return failed == 0 ? 0 : 1;
}
