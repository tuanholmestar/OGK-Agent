# OGK-Agent – Phase 2: LLM Client & Infrastructure
## Interface Specification v1.2 – Frozen / Approved

**Trạng thái:** Frozen / Approved  
**Phiên bản:** 1.2  
**Architecture:** v1.0  
**Ngôn ngữ:** C++23  
**Backend:** Ollama API (`/api/chat`)  
**HTTP Client:** libcurl  
**JSON Library:** nlohmann/json

---

# 1. Goals and Scope

## 1.1. Goals

Phase 2 xây dựng tầng giao tiếp giữa hệ thống C++ và LLM Backend.

Các mục tiêu:

1. Thiết kế abstract interface `LLMClient`.
2. Xác định request/response data model.
3. Hỗ trợ conversation history do caller quản lý.
4. Triển khai `OllamaClient`.
5. Hỗ trợ text input và image input nếu model thực tế hỗ trợ.
6. Hỗ trợ tool calling ở mức truyền và nhận dữ liệu, không thực thi tool.
7. Thiết kế Environment interface và xác định trách nhiệm.
8. Kiểm thử HTTP, JSON, error handling và tích hợp Ollama trên Kaggle.

## 1.2. Out of Scope

Không thực hiện trong Phase 2:

- AgentLoop và ReAct execution.
- ToolRegistry và Tool execution.
- Tool safety policy.
- SkillLoader và Skill selection.
- LoopDetector.
- HarnessRunner và Evaluator.
- Tự động quản lý conversation history.
- Asynchronous HTTP và streaming response.
- Image generation/output.

Tool calling chỉ được hỗ trợ ở mức data model và API request/response. Việc lựa chọn, xác thực và thực thi tool thuộc các phase sau.

---

# 2. Architecture Constraints

Phase 2 phải tuân thủ Architecture v1.0:

- `LLMClient` là abstract interface, không phụ thuộc riêng Ollama.
- `OllamaClient` kế thừa `LLMClient`.
- Tool implementation không phụ thuộc AgentLoop.
- AgentLoop không biết Harness tồn tại.
- Evaluator độc lập với cơ chế thực thi Agent.
- LLMClient đủ generic để có thể bổ sung backend khác.
- Không tự ý thay đổi kiến trúc đã phê duyệt.
- Ưu tiên RAII, smart pointers và YAGNI.

Interface Specification này cụ thể hóa public interface thuộc Phase 2, không thay thế Architecture v1.0.

---

# 3. LLM Data Model

Sử dụng namespace `ogk::llm`.

## 3.1. MessageRole

```cpp
enum class MessageRole {
    System,
    User,
    Assistant,
    Tool
};
```

Ý nghĩa:

- `System`: hướng dẫn hệ thống.
- `User`: nội dung người dùng cung cấp.
- `Assistant`: nội dung model tạo ra.
- `Tool`: kết quả trả về từ một tool đã thực thi.

## 3.2. ContentPart

```cpp
struct TextContent {
    std::string text;
};

struct ImageContent {
    std::string base64_data;
};

using ContentPart = std::variant<TextContent, ImageContent>;
```

`ContentPart` được giữ ở interface chung để hỗ trợ khả năng mở rộng sang backend có cách biểu diễn multimodal khác nhau.

### Ollama Mapping Rules

Ollama biểu diễn nội dung message thông qua trường `content` và mảng `images`.

Quy tắc chuyển đổi:

1. Các `TextContent` được lấy theo thứ tự xuất hiện.
2. Nhiều `TextContent` được nối bằng ký tự newline (`\n`).
3. Các `ImageContent` được gom vào mảng `images`.
4. Không bảo toàn thứ tự xen kẽ giữa text và image sau khi chuyển sang Ollama.
5. Không tự ý bỏ qua nội dung text hoặc image không hợp lệ.

Ví dụ:

```text
Input:
Text("Describe this image")
Image(...)
Text("Focus on the left side")

Ollama:
content = "Describe this image\nFocus on the left side"
images = [...]
```

**Known limitation:** Ollama mapping không giữ được cấu trúc xen kẽ text/image. Đây là giới hạn của backend mapping, không phải bảo đảm chung của interface.

Nếu sau này cần bảo toàn thứ tự multimodal, cần thiết kế adapter phù hợp với backend hỗ trợ cấu trúc đó.

`ImageContent` lưu Base64 data, không lưu file path. Việc đọc file và chuyển đổi sang Base64 thuộc trách nhiệm của tầng chuẩn bị dữ liệu.

---

# 4. Tool Calling Data Model

## 4.1. ToolCall

```cpp
struct ToolCall {
    std::optional<std::string> id;
    std::string name;
    nlohmann::json arguments;
};
```

- `id`: ID của tool call nếu backend cung cấp.
- `name`: tên tool được model yêu cầu.
- `arguments`: JSON object chứa tham số.

Đối với một `ToolCall` đã được LLMClient parse thành công, `arguments` phải là JSON object.

## 4.2. ToolDefinition

```cpp
struct ToolDefinition {
    std::string name;
    std::string description;
    nlohmann::json parameters;
};
```

`parameters` biểu diễn JSON Schema mô tả tham số của tool.

LLMClient chỉ chuyển ToolDefinition sang định dạng API của backend. Không chịu trách nhiệm xác thực tool arguments theo schema.

## 4.3. Tool Arguments Parsing Rules

LLMClient chịu trách nhiệm chuẩn hóa arguments nhận được từ backend.

Các trường hợp hợp lệ:

- Backend trả JSON object: sử dụng trực tiếp.
- Backend trả string chứa JSON object hợp lệ: parse string thành JSON object.

Các trường hợp không hợp lệ:

- String không parse được thành JSON.
- Kết quả parse không phải JSON object.
- Giá trị là array, number, boolean hoặc null thay vì object.

Các trường hợp không hợp lệ phải dẫn đến `ResponseParseError`.

## 4.4. Tool Arguments Serialization Validation

Khi caller tự dựng `ToolCall` để gửi lại trong conversation history, serializer phải kiểm tra:

```cpp
arguments.is_object()
```

Nếu không phải JSON object, ném `InvalidRequestError` trước khi gửi HTTP request.

Phân chia trách nhiệm:

- LLMClient parser: parse và chuẩn hóa JSON nhận được.
- LLMClient serializer: kiểm tra cấu trúc JSON cơ bản trước khi gửi.
- Tool System: validate JSON Schema.
- AgentLoop: quyết định có thực thi tool hay không.
- Tool implementation: xử lý nghiệp vụ.

LLMClient không được phụ thuộc ToolRegistry hoặc AgentLoop.

---

# 5. Message Model

```cpp
struct Message {
    MessageRole role;

    std::vector<ContentPart> content;

    std::vector<ToolCall> tool_calls;

    std::optional<std::string> tool_name;

    std::optional<std::string> tool_call_id;
};
```

Ý nghĩa:

- `role`: vai trò của message.
- `content`: nội dung text/image.
- `tool_calls`: tool calls do assistant tạo ra.
- `tool_name`: tên tool đã thực thi, dùng cho tool-result message.
- `tool_call_id`: ID của tool call tương ứng nếu có.

`tool_name` và `tool_call_id` là metadata tùy chọn. Backend không cung cấp ID vẫn có thể sử dụng interface.

## 5.1. Empty Message Rules

Không phải mọi message rỗng đều hợp lệ.

Các quy tắc:

1. `Assistant` message có `content` rỗng nhưng có ít nhất một `tool_call` là hợp lệ.
2. `Assistant` message có `content` rỗng và không có `tool_call` là không hợp lệ trong response.
3. `User` message không được có cả content rỗng và tool_calls rỗng.
4. `Tool` message phải có nội dung kết quả tool.
5. `System` message phải có nội dung hướng dẫn.

Ngoại lệ quan trọng:

Ollama có thể trả assistant message với `content: ""` khi model chỉ tạo tool calls. Parser phải chấp nhận trường hợp này, không được coi `content` rỗng là thiếu trường hoặc malformed JSON.

## 5.2. Conversation History Ownership

Conversation history được caller quản lý và truyền vào `ChatRequest`.

LLMClient:

- Không tự lưu lịch sử hội thoại.
- Không tự thêm system prompt.
- Không tự thêm tool result.
- Không tự thực thi tool.
- Chỉ serialize dữ liệu được caller cung cấp.

---

# 6. ChatOptions

```cpp
struct ChatOptions {
    std::optional<double> temperature;
    std::optional<int> max_tokens;

    std::optional<int> num_ctx;
    std::optional<double> top_p;
    std::optional<int> seed;
    std::vector<std::string> stop;
};
```

| Field | Ý nghĩa | Ollama mapping |
|---|---|---|
| temperature | Mức độ ngẫu nhiên | options.temperature |
| max_tokens | Giới hạn token đầu ra | options.num_predict |
| num_ctx | Context window size | options.num_ctx |
| top_p | Nucleus sampling | options.top_p |
| seed | Sampling seed | options.seed |
| stop | Chuỗi dừng sinh | options.stop |

Nguyên tắc:

1. Trường không được thiết lập sẽ không được gửi nếu không cần giá trị mặc định.
2. Giá trị phải được kiểm tra hợp lệ trước khi gửi request.
3. Không tự động áp đặt `temperature = 0`.
4. `seed` và `temperature = 0` có thể hỗ trợ kiểm thử tái lập, nhưng không đảm bảo kết quả tuyệt đối giống nhau giữa mọi môi trường.
5. `num_ctx` phải được cân nhắc theo khả năng phần cứng và model.
6. Backend khác có thể không hỗ trợ toàn bộ options. Adapter phải ghi nhận giới hạn thay vì âm thầm tuyên bố đã áp dụng.

---

# 7. ChatRequest

```cpp
struct ChatRequest {
    std::vector<Message> messages;

    std::vector<ToolDefinition> tools;

    ChatOptions options;
};
```

`ChatRequest` chứa dữ liệu cần thiết cho một lần gọi LLM.

- `messages`: conversation history.
- `tools`: danh sách tool definitions.
- `options`: generation parameters.

LLMClient không tự quản lý history và không thực thi tool.

---

# 8. ChatResponse

## 8.1. Usage

```cpp
struct Usage {
    std::optional<int> input_tokens;
    std::optional<int> output_tokens;
};
```

Mapping:

- `prompt_eval_count` → `input_tokens`.
- `eval_count` → `output_tokens`.

Nếu backend không cung cấp token usage, trường tương ứng giữ `std::nullopt`.

## 8.2. ChatResponse

```cpp
struct ChatResponse {
    Message message;

    std::optional<std::string> finish_reason;

    std::optional<Usage> usage;
};
```

Mapping:

- `message`: nội dung và tool calls do model trả về.
- `finish_reason`: ánh xạ từ `done_reason` nếu có.
- `usage`: token usage nếu có.

Phase 2 chỉ hỗ trợ text response và tool-call metadata. Không hỗ trợ image output.

## 8.3. Tool-call-only Response Example

Response JSON minh họa:

```json
{
  "model": "example-model",
  "message": {
    "role": "assistant",
    "content": "",
    "tool_calls": [
      {
        "function": {
          "name": "calculator",
          "arguments": {
            "expression": "15 * 17"
          }
        }
      }
    ]
  },
  "done": true,
  "done_reason": "stop",
  "prompt_eval_count": 120,
  "eval_count": 15
}
```

Parser phải:

- Chấp nhận `content` là chuỗi rỗng.
- Parse tool call.
- Chuẩn hóa arguments thành JSON object.
- Trả về `ChatResponse` có `message.role == Assistant`.
- Giữ `message.content` rỗng.
- Không tự thực thi calculator.

---

# 9. LLMClient Interface

```cpp
class LLMClient {
public:
    virtual ~LLMClient() = default;

    virtual ChatResponse chat(
        const ChatRequest& request
    ) = 0;
};
```

Yêu cầu:

- Abstract class.
- Synchronous API.
- Không chứa logic riêng của Ollama.
- Không quản lý conversation history.
- Không thực thi tool.
- Không phụ thuộc AgentLoop.
- Không phụ thuộc Harness.

---

# 10. OllamaClient

## 10.1. Configuration

```cpp
struct OllamaClientConfig {
    std::string base_url = "http://localhost:11434";

    std::string model;

    int connect_timeout_seconds = 10;

    int request_timeout_seconds = 300;
};
```

Ý nghĩa:

- `base_url`: địa chỉ Ollama server.
- `model`: tên model.
- `connect_timeout_seconds`: thời gian tối đa để thiết lập kết nối.
- `request_timeout_seconds`: thời gian tối đa cho toàn bộ request.

`model` phải không rỗng và được kiểm tra trong constructor của OllamaClient.

Nếu model rỗng, constructor ném `InvalidRequestError`.

Các timeout phải lớn hơn 0. Cấu hình không hợp lệ phải được phát hiện khi khởi tạo OllamaClient.

Giá trị mặc định là cấu hình khởi điểm và có thể thay đổi theo môi trường triển khai.

## 10.2. HTTP Integration

OllamaClient sử dụng HTTP POST.

Endpoint:

`/api/chat`

Request:

- JSON body.
- `stream: false`.
- Model name.
- Messages.
- Tools nếu được cung cấp.
- Options nếu được thiết lập.

HTTP được xử lý bằng libcurl.

## 10.3. Request Message Mapping

### System Message

```json
{
  "role": "system",
  "content": "You are a helpful assistant."
}
```

### User Message

```json
{
  "role": "user",
  "content": "Describe this image.",
  "images": ["BASE64_IMAGE_DATA"]
}
```

Nếu không có image, không cần gửi trường `images`.

### Assistant Message with Tool Calls

```json
{
  "role": "assistant",
  "content": "",
  "tool_calls": [
    {
      "function": {
        "name": "calculator",
        "arguments": {
          "expression": "15 * 17"
        }
      }
    }
  ]
}
```

Quy tắc:

- `role` là `assistant`.
- `content` chứa phần text đã mapping; có thể là chuỗi rỗng.
- `tool_calls` là mảng tool calls.
- Mỗi tool call được biểu diễn bằng `function.name` và `function.arguments`.
- `arguments` phải là JSON object.
- `id` chỉ được serialize khi backend/API mapping hỗ trợ trường tương ứng.

### Tool Result Message

```json
{
  "role": "tool",
  "tool_name": "calculator",
  "content": "255"
}
```

Quy tắc:

- `role` là `tool`.
- `tool_name` lấy từ `Message.tool_name`.
- `content` chứa kết quả tool.
- `tool_call_id` được truyền khi backend/API mapping hỗ trợ trường tương ứng.
- Tool result được caller đưa vào history sau khi tool đã thực thi.

Không tự tạo tool result trong OllamaClient.

**Compatibility note:** Các trường bổ sung như `tool_call_id` phải được kiểm chứng với phiên bản Ollama API thực tế. Không được giả định mọi phiên bản server chấp nhận cùng một schema.

## 10.4. Tool Definitions Mapping

ToolDefinition được chuyển thành cấu trúc tool mà Ollama API yêu cầu.

Ví dụ:

```json
{
  "type": "function",
  "function": {
    "name": "calculator",
    "description": "Evaluate arithmetic expressions.",
    "parameters": {
      "type": "object",
      "properties": {
        "expression": {
          "type": "string"
        }
      },
      "required": ["expression"]
    }
  }
}
```

## 10.5. JSON Serialization

OllamaClient chịu trách nhiệm:

- Chuyển ChatRequest sang JSON.
- Chuyển MessageRole sang role string.
- Mapping content và images.
- Mapping assistant tool calls.
- Mapping tool result messages.
- Mapping tool definitions.
- Mapping generation options.
- Validate request trước khi gửi.
- Serialize JSON.

Không để logic serialize/deserialize rò rỉ sang các tầng khác.

## 10.6. JSON Deserialization

OllamaClient chịu trách nhiệm:

- Parse response JSON.
- Kiểm tra cấu trúc response bắt buộc.
- Parse role và content.
- Parse tool calls.
- Chuẩn hóa arguments.
- Parse finish reason.
- Parse token usage.
- Chấp nhận assistant content rỗng nếu có tool calls.

Malformed JSON hoặc response structure không hợp lệ phải dẫn đến `ResponseParseError`.

## 10.7. Tool Calling

OllamaClient hỗ trợ:

- Gửi tool definitions.
- Nhận tool calls.
- Parse tên tool và arguments.
- Giữ tool call ID nếu backend cung cấp.
- Serialize assistant tool-call history.
- Serialize tool-result history.
- Chuyển dữ liệu thành ChatResponse.

Không thực thi tool.

Khả năng tool calling của model thực tế phải được kiểm chứng trong Integration Test.

## 10.8. Multimodal

OllamaClient hỗ trợ gửi image input thông qua Base64 nếu model thực tế hỗ trợ vision.

Yêu cầu:

- Không giả định mọi model đều hỗ trợ image input.
- Kiểm chứng model thực tế trước khi công bố tính năng.
- Ghi nhận giới hạn thứ tự xen kẽ text/image.
- Không hỗ trợ image generation/output trong Phase 2.

---

# 11. Error Handling

Sử dụng exception-based error handling.

## 11.1. Exception Hierarchy

Các exception kế thừa từ `LLMError`.

```cpp
class LLMError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
```

### TimeoutKind

`TimeoutKind` phải được khai báo trước `TimeoutError`.

```cpp
enum class TimeoutKind {
    Connect,
    Request
};
```

### TimeoutError

```cpp
class TimeoutError : public LLMError {
public:
    TimeoutError(
        TimeoutKind kind,
        const std::string& message
    );

    TimeoutKind kind() const noexcept;

private:
    TimeoutKind kind_;
};
```

`TimeoutError` phải lưu loại timeout để caller có thể phân biệt Connect timeout và Request timeout.

### ApiError

```cpp
class ApiError : public LLMError {
public:
    ApiError(
        int status_code,
        const std::string& message,
        const std::string& response_body
    );

    int status_code() const noexcept;

    const std::string& response_body() const noexcept;

private:
    int status_code_;
    std::string response_body_;
};
```

`ApiError` lưu:

- HTTP status code.
- Error message.
- Response body đã được cắt ngắn.

Giới hạn response body lưu trong exception: tối đa 4096 bytes.

Nếu body dài hơn giới hạn, chỉ lưu phần đầu và đánh dấu nội dung đã bị cắt.

Không lưu API key, token hoặc thông tin nhạy cảm trong error message/body.

### Other Exceptions

```cpp
class ConnectionError : public LLMError {
public:
    using LLMError::LLMError;
};

class ResponseParseError : public LLMError {
public:
    using LLMError::LLMError;
};

class InvalidRequestError : public LLMError {
public:
    using LLMError::LLMError;
};
```

## 11.2. Error Mapping

| Tình huống | Exception |
|---|---|
| Không thiết lập được kết nối | ConnectionError |
| Connect timeout | TimeoutError (Connect) |
| Request timeout | TimeoutError (Request) |
| Response JSON không hợp lệ | ResponseParseError |
| Response structure không hợp lệ | ResponseParseError |
| Arguments không chuẩn hóa được | ResponseParseError |
| HTTP/API trả lỗi | ApiError |
| Request vi phạm validation | InvalidRequestError |
| OllamaClient config không hợp lệ | InvalidRequestError |

HTTP status code phải được lưu trong ApiError để caller phân biệt các lỗi như 400, 404 và 500.

Không tự động retry trong Phase 2.

Không để lỗi bị nuốt hoặc chuyển thành response thành công giả.

---

# 12. Request Validation Rules

Validation được thực hiện trước khi gửi HTTP request.

Các lỗi validation phải ném `InvalidRequestError`.

## 12.1. OllamaClient Configuration

Kiểm tra trong constructor:

| Field | Rule |
|---|---|
| base_url | Không được rỗng |
| model | Không được rỗng |
| connect_timeout_seconds | Phải > 0 |
| request_timeout_seconds | Phải > 0 |

Model rỗng phải được phát hiện trong constructor, không đợi đến `chat()`.

## 12.2. ChatRequest

| Field | Validation rule |
|---|---|
| messages | Không được rỗng |
| temperature | Nếu có, nằm trong [0.0, 2.0] |
| max_tokens | Nếu có, phải > 0 |
| num_ctx | Nếu có, phải > 0 |
| top_p | Nếu có, nằm trong (0.0, 1.0] |
| seed | Nếu có, phải >= 0 |
| stop | Các chuỗi không được rỗng |

Giới hạn `temperature` được chọn làm validation range của interface cho Ollama adapter. Backend khác có thể có giới hạn riêng và phải kiểm tra lại trong adapter tương ứng.

## 12.3. ToolDefinition

| Field | Validation rule |
|---|---|
| name | Không được rỗng |
| description | Không được rỗng |
| parameters | Phải là JSON object |

## 12.4. Message

Validation tối thiểu:

- `System` message phải có nội dung text không rỗng.
- `User` message không được có cả content rỗng và tool_calls rỗng.
- `Assistant` message có content rỗng chỉ hợp lệ khi có tool_calls.
- `Tool` message phải có tool result content.
- `ImageContent.base64_data` không được rỗng.
- `Assistant` tool calls phải có tên tool không rỗng.
- Mọi `ToolCall.arguments` phải là JSON object khi serialize.
- `Tool` message phải có `tool_name` không rỗng.

Validation không kiểm tra JSON Schema của từng tool.

---

# 13. Resource Management

Áp dụng RAII:

- Quản lý CURL handle bằng RAII wrapper.
- Giải phóng tài nguyên đúng vòng đời.
- Không dùng raw owning pointer.
- Ưu tiên `std::unique_ptr` khi cần dynamic ownership.
- Không giữ tài nguyên HTTP lâu hơn cần thiết.
- Không để memory leak khi exception xảy ra.

---

# 14. Dependencies

## 14.1. libcurl

Dùng cho HTTP communication.

## 14.2. nlohmann/json

Dùng cho JSON serialization/deserialization.

Do `nlohmann::json` xuất hiện trong public data model, đây là public dependency.

## 14.3. Dependency Management

- Ưu tiên `find_package()` khi môi trường cung cấp package phù hợp.
- Không thêm dependency không cần thiết.
- Không tự ý đưa dependency mới vào kiến trúc mà chưa đánh giá tác động.

---

# 15. Environment Design

Phase 2 chỉ thiết kế interface và trách nhiệm. Không triển khai Environment implementation trong phase này.

## 15.1. Environment

Environment đại diện cho môi trường mà Tool có thể tương tác.

Trách nhiệm cần xem xét:

- Xác định phạm vi thao tác được phép.
- Cung cấp cơ chế truy cập tài nguyên.
- Quản lý cấu hình môi trường.
- Hỗ trợ giới hạn quyền truy cập.
- Đảm bảo tương thích với Tool System.

Environment không phụ thuộc AgentLoop.

## 15.2. NativeEnvironment

Đại diện môi trường chạy trực tiếp trên máy host.

Cần xác định:

- Phạm vi truy cập file.
- Quyền thao tác.
- Working directory.
- Xử lý lỗi hệ thống.

## 15.3. SandboxEnvironment

Đại diện môi trường thực thi bị giới hạn.

Cần xác định:

- Phạm vi tài nguyên được phép truy cập.
- Cơ chế giới hạn thao tác.
- Cách xử lý vi phạm policy.
- Cơ chế isolation nếu môi trường hỗ trợ.

Không tuyên bố SandboxEnvironment an toàn tuyệt đối nếu chưa có cơ chế isolation thực tế.

## 15.4. Phase 2 Decision

Phase 2 chỉ xác định trách nhiệm và đề xuất interface tương thích Tool System.

Việc triển khai NativeEnvironment và SandboxEnvironment được xem xét ở phase sau cùng Tool System.

Không tạo implementation rỗng hoặc placeholder.

---

# 16. Testing Strategy

## 16.1. Unit Tests

Kiểm tra:

- MessageRole mapping.
- Content serialization.
- Text/image mapping.
- Multiple TextContent joining.
- Empty assistant message with tool calls.
- Invalid empty message.
- Assistant tool-call serialization.
- Tool result serialization.
- ToolDefinition serialization.
- ToolCall parsing.
- Arguments object parsing.
- Arguments string-to-object parsing.
- Invalid arguments handling.
- Arguments serialization validation.
- ChatOptions mapping.
- Usage parsing.
- Finish reason parsing.
- Request validation.
- TimeoutError kind.
- ApiError status code.
- ApiError response body truncation.

## 16.2. HTTP Tests

Kiểm tra:

- Successful HTTP POST.
- Connection refused.
- Connect timeout.
- Request timeout.
- HTTP 400.
- HTTP 404.
- HTTP 500.
- Error response body truncation.
- Malformed JSON.
- Empty response.
- Invalid response structure.

Có thể sử dụng mock HTTP server hoặc test double để kiểm tra độc lập với model thật.

## 16.3. Integration Tests

Kiểm chứng:

- Kết nối Ollama server.
- Model thực tế có sẵn.
- Text-only request.
- Conversation history.
- Assistant tool-call history.
- Tool-result history.
- Tool calling request/response.
- Assistant response có content rỗng và tool calls.
- Image input nếu model hỗ trợ.
- Generation options.
- Token usage.
- Giới hạn context và hành vi khi prompt dài.

Integration test với Kaggle cần ghi nhận model, cấu hình và môi trường thực tế.

Không đưa API key, token hoặc thông tin nhạy cảm vào GitHub.

---

# 17. Acceptance Criteria

Phase 2 chỉ được xem là hoàn thành khi:

1. LLMClient abstract interface được triển khai đúng.
2. Request/response model được định nghĩa và kiểm thử.
3. OllamaClient gửi HTTP POST thành công.
4. JSON serialization/deserialization hoạt động.
5. Có xử lý timeout và connection error.
6. TimeoutError phân biệt được Connect và Request.
7. ApiError lưu HTTP status code và response body đã giới hạn.
8. Có xử lý malformed JSON.
9. Có request validation.
10. Có xử lý tool-call response ở mức data model.
11. Assistant tool-call history và tool-result history được serialize đúng.
12. Assistant response có content rỗng nhưng chứa tool calls được parser chấp nhận.
13. Có hỗ trợ generation options đã thống nhất.
14. Image input được kiểm chứng nếu model hỗ trợ.
15. Environment interface được thiết kế và có tài liệu trách nhiệm.
16. Unit tests và integration tests có kết quả ghi nhận.
17. Có tài liệu usage và known issues.
18. Không triển khai AgentLoop trong Phase 2.

---

# 18. Proposed Directory Structure

Chỉ tạo file/thư mục khi có implementation thực tế.

```text
OGK-Agent/
├── include/
│   └── ogk/
│       └── llm/
│           ├── llm_client.hpp
│           ├── message.hpp
│           ├── chat_request.hpp
│           ├── chat_response.hpp
│           ├── chat_options.hpp
│           ├── tool_call.hpp
│           ├── tool_definition.hpp
│           ├── ollama_client.hpp
│           └── llm_errors.hpp
│
├── src/
│   └── llm/
│       ├── ollama_client.cpp
│       ├── ollama_serializer.cpp
│       └── ollama_parser.cpp
│
├── tests/
│   └── llm/
│       ├── test_message.cpp
│       ├── test_ollama_serializer.cpp
│       ├── test_ollama_parser.cpp
│       ├── test_ollama_client.cpp
│       └── test_integration.cpp
│
└── docs/
    ├── phase-2-interface-spec.md
    └── phase-2-handover.md
```

Có thể điều chỉnh cách chia file khi triển khai nếu không thay đổi public contract đã duyệt.

---

# 19. Known Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Model không hỗ trợ tool calling | Không thể sử dụng tool | Kiểm chứng model thực tế |
| Model không hỗ trợ vision | Không dùng được image input | Kiểm tra model capability |
| Context window nhỏ | Mất một phần conversation history | Cấu hình num_ctx và kiểm thử prompt dài |
| Model trả arguments sai định dạng | Không thể sử dụng tool call | Chuẩn hóa và báo lỗi |
| Ollama API thay đổi | Adapter không tương thích | Kiểm tra API version |
| Kaggle server không ổn định | Integration test thất bại | Ghi nhận môi trường và lỗi |
| Timeout quá ngắn | Request thất bại khi model load lâu | Tách connect/request timeout |
| Backend không hỗ trợ options | Cấu hình không có tác dụng | Ghi nhận capability và giới hạn |
| Tool history mapping không tương thích | Conversation history không tiếp tục đúng | Kiểm thử request/response thực tế |

---

# 20. Phase 2 Deliverables

1. Interface Specification.
2. LLMClient abstract interface.
3. OllamaClient implementation.
4. Request/response data model.
5. JSON serializer/parser.
6. Error handling.
7. Request validation.
8. Environment design.
9. Unit test cases.
10. Integration test cases và kết quả.
11. Known Issues.
12. Usage Guide.
13. Handover Report.

---

# 21. Decisions Register

| ID | Decision | Status |
|---|---|---|
| D01 | Caller owns conversation history | Approved |
| D02 | OllamaClientConfig owns base_url/model | Approved |
| D03 | ChatOptions owns generation parameters | Approved |
| D04 | Exception-based error handling | Approved |
| D05 | Message supports multimodal extension | Approved |
| D06 | ImageContent stores Base64 | Approved |
| D07 | Synchronous chat() | Approved |
| D08 | Phase 2 response is text-only; image input may be supported | Approved |
| D09 | Tool calling is supported at data-model/API level, not executed | Approved |
| D10 | ToolCall.arguments uses nlohmann::json | Approved |
| D11 | ToolDefinition is included in ChatRequest | Approved |
| D12 | nlohmann::json is a public dependency | Approved |
| D13 | Environment is design-only in Phase 2 | Approved |
| D14 | Keep ContentPart variant; Ollama joins text with newline and collects images separately | Approved |
| D15 | Add optional tool_call_id to Message | Approved |
| D16 | LLMClient normalizes arguments but does not validate tool schema | Approved |
| D17 | JSON string arguments may be parsed into an object; invalid/non-object values cause ResponseParseError | Approved |
| D18 | Separate connect_timeout and request_timeout | Approved |
| D19 | TimeoutError distinguishes Connect and Request and stores TimeoutKind | Approved |
| D20 | Add num_ctx, top_p, seed and stop to ChatOptions | Approved |
| D21 | Environment implementations are deferred to a later phase | Approved |
| D22 | ApiError stores HTTP status code and truncated response body | Approved |
| D23 | Define explicit request validation rules | Approved |
| D24 | OllamaClient validates non-empty model in constructor | Approved |
| D25 | Serializer validates ToolCall.arguments is a JSON object | Approved |
| D26 | Define explicit assistant tool-call and tool-result history mapping | Approved |
| D27 | Assistant message with empty content and non-empty tool_calls is valid | Approved |

---

**End of Interface Specification v1.2 – Frozen / Approved**

This specification is frozen and approved. Changes require explicit approval.

