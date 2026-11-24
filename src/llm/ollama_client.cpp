
#include <ogk/llm/ollama_client.hpp>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

namespace ogk::llm {
namespace {

using json = nlohmann::json;

// Đảm bảo libcurl được khởi tạo một lần.
void initialize_curl()
{
    static std::once_flag flag;
    static CURLcode result = CURLE_OK;

    std::call_once(flag, [] {
        result = curl_global_init(CURL_GLOBAL_DEFAULT);
    });

    if (result != CURLE_OK) {
        throw ConnectionError("Failed to initialize libcurl");
    }
}

struct CurlDeleter {
    void operator()(CURL* handle) const noexcept
    {
        if (handle) {
            curl_easy_cleanup(handle);
        }
    }
};

struct CurlListDeleter {
    void operator()(curl_slist* list) const noexcept
    {
        if (list) {
            curl_slist_free_all(list);
        }
    }
};

using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;
using CurlList = std::unique_ptr<curl_slist, CurlListDeleter>;

size_t write_callback(
    char* data,
    size_t size,
    size_t count,
    void* user_data
) noexcept
{
    const size_t bytes = size * count;

    auto* response = static_cast<std::string*>(user_data);

    try {
        response->append(data, bytes);
        return bytes;
    }
    catch (...) {
        return 0;
    }
}

std::string make_url(const std::string& base_url)
{
    std::string url = base_url;

    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }

    return url + "/api/chat";
}

const char* role_to_string(MessageRole role)
{
    switch (role) {
    case MessageRole::System:
        return "system";
    case MessageRole::User:
        return "user";
    case MessageRole::Assistant:
        return "assistant";
    case MessageRole::Tool:
        return "tool";
    }

    throw InvalidRequestError("Unsupported message role");
}

json serialize_message(const Message& message)
{
    json result;
    result["role"] = role_to_string(message.role);

    std::string content;
    json images = json::array();

    for (const auto& part : message.content) {
        std::visit(
            [&](const auto& item) {
                using T = std::decay_t<decltype(item)>;

                if constexpr (std::is_same_v<T, TextContent>) {
                    content += item.text;
                }
                else if constexpr (std::is_same_v<T, ImageContent>) {
                    images.push_back(item.base64_data);
                }
            },
            part
        );
    }

    result["content"] = content;

    if (!images.empty()) {
        result["images"] = std::move(images);
    }

    if (message.role == MessageRole::Tool && message.tool_name) {
        result["name"] = *message.tool_name;
    }

    if (message.role == MessageRole::Assistant &&
        !message.tool_calls.empty()) {

        json calls = json::array();

        for (const auto& tool_call : message.tool_calls) {
            calls.push_back({
                {"type", "function"},
                {"function", {
                    {"name", tool_call.name},
                    {"arguments", tool_call.arguments}
                }}
            });
        }

        result["tool_calls"] = std::move(calls);
    }

    return result;
}

json serialize_request(
    const ChatRequest& request,
    const OllamaClientConfig& config
)
{
    json body;

    body["model"] = config.model;
    body["stream"] = false;

    body["messages"] = json::array();

    for (const auto& message : request.messages) {
        body["messages"].push_back(serialize_message(message));
    }

    if (!request.tools.empty()) {
        body["tools"] = json::array();

        for (const auto& tool : request.tools) {
            body["tools"].push_back({
                {"type", "function"},
                {"function", {
                    {"name", tool.name},
                    {"description", tool.description},
                    {"parameters", tool.parameters}
                }}
            });
        }
    }

    json options = json::object();

    if (request.options.temperature) {
        options["temperature"] = *request.options.temperature;
    }

    if (request.options.max_tokens) {
        options["num_predict"] = *request.options.max_tokens;
    }

    if (request.options.num_ctx) {
        options["num_ctx"] = *request.options.num_ctx;
    }

    if (request.options.top_p) {
        options["top_p"] = *request.options.top_p;
    }

    if (request.options.seed) {
        options["seed"] = *request.options.seed;
    }

    if (!request.options.stop.empty()) {
        options["stop"] = request.options.stop;
    }

    if (!options.empty()) {
        body["options"] = std::move(options);
    }

    return body;
}

MessageRole role_from_string(const std::string& role)
{
    if (role == "system") {
        return MessageRole::System;
    }
    if (role == "user") {
        return MessageRole::User;
    }
    if (role == "assistant") {
        return MessageRole::Assistant;
    }
    if (role == "tool") {
        return MessageRole::Tool;
    }

    throw ResponseParseError("Unknown message role: " + role);
}

ChatResponse parse_response(const json& body)
{
    if (!body.is_object() ||
        !body.contains("message") ||
        !body["message"].is_object()) {
        throw ResponseParseError(
            "Ollama response does not contain a valid message"
        );
    }

    const auto& message_json = body["message"];

    if (!message_json.contains("role") ||
        !message_json["role"].is_string()) {
        throw ResponseParseError("Missing or invalid message role");
    }

    Message message{
        role_from_string(message_json["role"].get<std::string>()),
        {},
        {},
        std::nullopt,
        std::nullopt
    };

    if (message_json.contains("content")) {
        if (!message_json["content"].is_string()) {
            throw ResponseParseError("Invalid message content");
        }

        const std::string content =
            message_json["content"].get<std::string>();

        if (!content.empty()) {
            message.content.push_back(TextContent{content});
        }
    }

    if (message_json.contains("tool_calls")) {
        if (!message_json["tool_calls"].is_array()) {
            throw ResponseParseError("Invalid tool_calls format");
        }

        for (const auto& call : message_json["tool_calls"]) {
            if (!call.contains("function") ||
                !call["function"].is_object()) {
                throw ResponseParseError("Invalid tool call");
            }

            const auto& function = call["function"];

            if (!function.contains("name") ||
                !function["name"].is_string()) {
                throw ResponseParseError("Missing tool call name");
            }
            if (!function.contains("arguments") ||
                !function["arguments"].is_object()) {
                throw ResponseParseError(
                    "Missing or invalid tool call arguments"
                );
}

            message.tool_calls.push_back({
                std::nullopt,
                function["name"].get<std::string>(),
                function["arguments"]
            });
        }
    }

    ChatResponse response{
        std::move(message),
        std::nullopt,
        std::nullopt
    };

    if (body.contains("done_reason") &&
        body["done_reason"].is_string()) {
        response.finish_reason =
            body["done_reason"].get<std::string>();
    }

    if (body.contains("prompt_eval_count") ||
        body.contains("eval_count")) {

        Usage usage;

        if (body.contains("prompt_eval_count") &&
            body["prompt_eval_count"].is_number_integer()) {
            usage.input_tokens =
                body["prompt_eval_count"].get<int>();
        }

        if (body.contains("eval_count") &&
            body["eval_count"].is_number_integer()) {
            usage.output_tokens =
                body["eval_count"].get<int>();
        }

        response.usage = usage;
    }

    return response;
}

} // namespace

ChatResponse OllamaClient::chat(const ChatRequest& request)
{
    if (request.messages.empty()) {
        throw InvalidRequestError(
            "ChatRequest must contain at least one message"
        );
    }

    initialize_curl();

    CurlHandle curl(curl_easy_init());

    if (!curl) {
        throw ConnectionError("Failed to create CURL handle");
    }

    const std::string url = make_url(config_.base_url);
    const std::string request_body =
        serialize_request(request, config_).dump();

    std::string response_body;
    char error_buffer[CURL_ERROR_SIZE] = {};

    CurlList headers(
        curl_slist_append(nullptr, "Content-Type: application/json")
    );

    if (!headers) {
        throw ConnectionError("Failed to create HTTP headers");
    }

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());

    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(
        curl.get(),
        CURLOPT_POSTFIELDS,
        request_body.c_str()
    );
    curl_easy_setopt(
        curl.get(),
        CURLOPT_POSTFIELDSIZE_LARGE,
        static_cast<curl_off_t>(request_body.size())
    );

    curl_easy_setopt(
        curl.get(),
        CURLOPT_CONNECTTIMEOUT,
        static_cast<long>(config_.connect_timeout_seconds)
    );

    curl_easy_setopt(
        curl.get(),
        CURLOPT_TIMEOUT,
        static_cast<long>(config_.request_timeout_seconds)
    );

    curl_easy_setopt(
        curl.get(),
        CURLOPT_WRITEFUNCTION,
        write_callback
    );
    curl_easy_setopt(
        curl.get(),
        CURLOPT_WRITEDATA,
        &response_body
    );

    curl_easy_setopt(
        curl.get(),
        CURLOPT_ERRORBUFFER,
        error_buffer
    );

    const CURLcode result = curl_easy_perform(curl.get());

        if (result != CURLE_OK) {
        const std::string error_message =
            error_buffer[0] != '\0'
                ? error_buffer
                : curl_easy_strerror(result);

        // Lỗi ghi response body
        if (result == CURLE_WRITE_ERROR) {
            throw LLMError("Failed to write Ollama response body");
        }

        // Lỗi timeout
        if (result == CURLE_OPERATION_TIMEDOUT) {
            double connect_time = 0.0;

            curl_easy_getinfo(
                curl.get(),
                CURLINFO_CONNECT_TIME,
                &connect_time
            );

            const TimeoutKind kind =
                connect_time <= 0.0
                    ? TimeoutKind::Connect
                    : TimeoutKind::Request;

            throw TimeoutError(kind, error_message);
        }

        // Lỗi kết nối
        if (result == CURLE_COULDNT_CONNECT ||
            result == CURLE_COULDNT_RESOLVE_HOST ||
            result == CURLE_COULDNT_RESOLVE_PROXY ||
            result == CURLE_RECV_ERROR ||
            result == CURLE_SEND_ERROR ||
            result == CURLE_GOT_NOTHING) {
            throw ConnectionError(error_message);
        }

        throw LLMError(error_message);
    }

    long status_code = 0;

    curl_easy_getinfo(
        curl.get(),
        CURLINFO_RESPONSE_CODE,
        &status_code
    );
    if (status_code < 200 || status_code >= 300) {
        throw ApiError(
            static_cast<int>(status_code),
            "Ollama API returned HTTP error",
            response_body
        );
    }

    try {
        const json body = json::parse(response_body);
        return parse_response(body);
    }
    catch (const nlohmann::json::exception& error) {
        throw ResponseParseError(
            std::string("Failed to parse Ollama response: ") +
            error.what()
        );
    }
}

} // namespace ogk::llm
