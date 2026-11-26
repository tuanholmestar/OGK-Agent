# Phase 3 Interface Specification

**Version:** 1.0 — Approved Interface Specification  
**Status:** Approved  
**Language:** C++23

---

## 1. Core Types

### 1.1. JSON Data Type

Sử dụng trực tiếp `nlohmann::json` làm kiểu JSON thống nhất trong Phase 3.

Cơ sở: Phase 2 đã chốt `nlohmann/json` là public dependency tại Decision D10 và D12.

Không định nghĩa alias `Json` riêng trong namespace `ogk::tools` hoặc `ogk::llm`, nhằm duy trì tính nhất quán với interface Phase 2.

Các interface trong Phase 3 sử dụng:

```cpp
nlohmann::json
```

### 1.2. ErrorCode

Định nghĩa enum biểu diễn các loại lỗi trong Tool System.

```cpp
namespace ogk::tools
{
    enum class ErrorCode
    {
        InvalidArguments,
        PermissionDenied,
        ResourceNotFound,
        ResourceUnavailable,
        ExecutionFailed,
        Timeout,
        InternalError,
        InteractionUnavailable
    };

    [[nodiscard]]
    std::string_view to_string(ErrorCode code) noexcept;
}
```

Ý nghĩa:

| ErrorCode | Mã chuỗi ổn định | Ý nghĩa |
|---|---|---|
| `InvalidArguments` | `INVALID_ARGUMENTS` | Arguments không hợp lệ |
| `PermissionDenied` | `PERMISSION_DENIED` | Không được phép thực hiện |
| `ResourceNotFound` | `RESOURCE_NOT_FOUND` | Không tìm thấy Tool hoặc tài nguyên |
| `ResourceUnavailable` | `RESOURCE_UNAVAILABLE` | Tài nguyên hoặc dịch vụ tạm thời không khả dụng |
| `ExecutionFailed` | `EXECUTION_FAILED` | Thực thi thất bại |
| `Timeout` | `TIMEOUT` | Quá thời gian cho phép |
| `InternalError` | `INTERNAL_ERROR` | Lỗi nội bộ |
| `InteractionUnavailable` | `INTERACTION_UNAVAILABLE` | Không có kênh tương tác người dùng |

**Quy tắc `to_string()`:**

- Trả về `std::string_view` trỏ đến string literal có static storage duration.
- Không cấp phát bộ nhớ.
- Không trả về view trỏ đến biến cục bộ hoặc chuỗi tạm.
- Các giá trị chuỗi ở bảng trên là contract ổn định được dùng trong serialization và giao tiếp với LLM.
- Không triển khai `from_string()` trong Phase 3 vì chưa có yêu cầu đọc ErrorCode từ chuỗi.

### 1.3. ErrorInfo

`ErrorInfo` lưu thông tin lỗi có cấu trúc.

```cpp
namespace ogk::tools
{
    struct ErrorInfo
    {
        ErrorCode code;
        std::string message;
        std::optional<std::string> field_path;
    };
}
```

**Các trường:**

- `code`: loại lỗi.
- `message`: thông báo lỗi dành cho tầng gọi Tool.
- `field_path`: đường dẫn đến trường gây lỗi, nếu có.

`field_path` sử dụng JSON Pointer theo RFC 6901.

Quy ước:

- Gốc của JSON Pointer là chính object `arguments` truyền vào Tool.
- `""` biểu diễn toàn bộ object arguments.
- `/tags/1` biểu diễn phần tử thứ hai của field `tags`.
- `/arguments/0` biểu diễn phần tử đầu tiên của field `arguments`, ví dụ với input của Tool `exec`.

Không bắt buộc có `field_path` đối với lỗi không liên quan đến một trường input cụ thể.

Không đưa exception message hoặc thông tin kỹ thuật nội bộ nhạy cảm trực tiếp cho LLM.

### 1.4. ToolResult

`ToolResult` biểu diễn kết quả thực thi của một Tool.

Thiết kế sử dụng `class` với constructor private. Trạng thái Success/Error được suy ra từ `error_`, không lưu thêm một biến `success` độc lập.

```cpp
namespace ogk::tools
{
    class ToolResult
    {
    public:
        [[nodiscard]]
        bool success() const noexcept
        {
            return !error_.has_value();
        }

        const nlohmann::json& data() const noexcept
        {
            return data_;
        }

        const std::optional<ErrorInfo>& error() const noexcept
        {
            return error_;
        }

        [[nodiscard]]
        static ToolResult make_success(
            nlohmann::json data = nlohmann::json::object()
        );

        [[nodiscard]]
        static ToolResult make_error(ErrorInfo error);

        [[nodiscard]]
        static ToolResult make_error(
            ErrorCode code,
            std::string message,
            std::optional<std::string> field_path = std::nullopt
        );

    private:
        ToolResult(
            nlohmann::json data,
            std::optional<ErrorInfo> error
        );

        nlohmann::json data_;
        std::optional<ErrorInfo> error_;
    };
}
```

**Quy tắc invariant:**

| Trạng thái | `success()` | `data()` | `error()` |
|---|---|---|---|
| Success | `true` | JSON object | `nullopt` |
| Error | `false` | JSON null | Có ErrorInfo |

**Quy tắc dữ liệu Success:**

- Success luôn chứa một JSON object.
- Tool không có dữ liệu trả về sử dụng `{}`.
- Không sử dụng JSON `null`, scalar hoặc array làm payload trực tiếp của Success.
- `make_success()` phải từ chối payload không phải JSON object bằng `std::invalid_argument`.
- Các giá trị scalar hoặc array cần được đặt trong một object có field phù hợp. Ví dụ: `{"result": 255}`.

**Quy tắc Error:**

- Error luôn có `ErrorInfo`.
- `data_` của Error luôn là JSON null.
- Không tồn tại trạng thái vừa Success vừa có ErrorInfo.

**Copy/Move semantics:**

- Giữ default copy constructor và copy assignment.
- Giữ default move constructor và move assignment.
- Không tự định nghĩa destructor.
- Cho phép trả `ToolResult` theo giá trị và lưu trong container như `std::vector`.

Các factory function là cách duy nhất để tạo `ToolResult` từ bên ngoài class.

### 1.5. Tool Interface

Mọi Tool phải triển khai interface thống nhất.

```cpp
namespace ogk::tools
{
    class Tool
    {
    public:
        virtual ~Tool() = default;

        virtual std::string_view name() const noexcept = 0;

        virtual std::string description() const = 0;

        virtual nlohmann::json parameters() const = 0;

        virtual ToolResult execute(
            const nlohmann::json& arguments
        ) = 0;
    };
}
```

**Trách nhiệm từng hàm:**

| Hàm | Trách nhiệm |
|---|---|
| `name()` | Trả về tên duy nhất của Tool |
| `description()` | Trả về mô tả chức năng cho LLM |
| `parameters()` | Trả về JSON Schema mô tả arguments |
| `execute()` | Thực thi Tool và trả về ToolResult |

**Quy tắc `name()`:**

- Trả về `std::string_view` để tránh copy khi Registry tra cứu và sắp xếp.
- Tên phải có lifetime ít nhất bằng lifetime của Tool.
- Implementation lưu tên trong member `std::string` của instance hoặc dùng string literal tĩnh.
- Không trả `string_view` trỏ tới biến cục bộ, biến tạm hoặc kết quả của một phép dựng chuỗi tạm thời.
- Với `FileTool` và `MemoryTool`, mỗi instance lưu tên riêng để phân biệt các operation.

**Quy tắc `description()`:**

- Trả về `std::string` theo giá trị.
- Cho phép implementation dựng description động nếu cần.
- Chi phí copy được chấp nhận vì description không được gọi thường xuyên như `name()`.

**Quy tắc `parameters()`:**

- Trả về `nlohmann::json` theo giá trị.
- Kết quả phải là JSON object.
- Schema bắt buộc có `"type": "object"` ở cấp cao nhất.
- Schema phải ổn định giữa các lần gọi.
- ToolRegistry có thể cache schema sau khi đăng ký thành công.
- Schema không được thay đổi tùy tiện trong lifetime đăng ký của Tool.

ToolRegistry kiểm tra tính hợp lệ của schema khi đăng ký. Schema không hợp lệ là lỗi cấu hình và phải ném exception.

**Quy tắc `execute()`:**

1. Tool không phụ thuộc AgentLoop hoặc Harness.
2. Tool không tự kiểm tra ToolPolicy.
3. Tool không tự thực hiện JSON Schema validation cấp Registry.
4. Tool nhận arguments dưới dạng `const nlohmann::json&`.
5. Tool trả `ToolResult` thay vì ném exception để biểu diễn lỗi nghiệp vụ có thể dự đoán.
6. Tool vẫn phải tự kiểm tra các ràng buộc nghiệp vụ không thể biểu diễn đầy đủ bằng JSON Schema. Ví dụ: field bắt buộc phụ thuộc operation trong `todo_list`.
7. Tool được phép ném exception khi gặp lỗi không lường trước hoặc lỗi lập trình. ToolRegistry chịu trách nhiệm bắt exception và chuyển thành `InternalError`; không gửi raw `e.what()` cho LLM.
8. `execute()` không khai báo `const` vì một số Tool cần thay đổi trạng thái nội bộ, ví dụ TodoListTool.

### 1.6. Thread Safety

Phase 3 sử dụng mô hình thực thi đồng bộ.

- `Tool::execute()` là non-const.
- ToolRegistry không cam kết thread-safe.
- Không hỗ trợ nhiều thread đồng thời truy cập và thực thi trên cùng Registry.
- Nếu cần concurrent execution trong tương lai, thread safety sẽ được thiết kế ở phase riêng, không mở rộng phạm vi Phase 3 hiện tại.

---

## 2. Core Types — Approved Decisions

| ID | Quyết định | Trạng thái |
|---|---|---|
| CT-01 | Sử dụng trực tiếp `nlohmann::json`, tham chiếu D10/D12 | Approved |
| CT-02 | ErrorCode dùng PascalCase | Approved |
| CT-03 | ErrorCode có `to_string()` với mã chuỗi ổn định | Approved |
| CT-04 | Bổ sung `ResourceUnavailable` | Approved |
| CT-05 | ToolResult dùng private constructor và factory functions | Approved |
| CT-06 | Trạng thái Success/Error suy ra từ `error_` | Approved |
| CT-07 | Success payload bắt buộc là JSON object | Approved |
| CT-08 | Tool::name() trả `std::string_view` | Approved |
| CT-09 | Tool::execute() non-const | Approved |
| CT-10 | Registry đồng bộ, không cam kết thread-safe | Approved |
| CT-11 | Registry cache JSON Schema sau khi đăng ký | Approved |

# Section 2 – ToolPolicy & JSON Schema Validator

**Version:** 1.0  
**Status:** Approved — Phase 3 Interface Specification  
**Scope:** Tool permission policy and JSON Schema subset validation  
**Dependencies:** Section 1 – Core Types

---

## 2.1. Mục tiêu và phạm vi

Section này định nghĩa hai thành phần nền tảng của Tool Registry:

1. **ToolPolicy:** quyết định Tool nào được phép thực thi.
2. **JsonSchemaValidator:** kiểm tra `arguments` có đúng với JSON Schema mà Tool khai báo hay không.

Hai thành phần này được sử dụng trong luồng thực thi Tool của Registry:

1. Tìm Tool theo tên.
2. Kiểm tra quyền bằng ToolPolicy.
3. Kiểm tra arguments bằng JsonSchemaValidator.
4. Gọi `Tool::execute()`.
5. Chuyển kết quả hoặc lỗi về dạng `ToolResult`.

Section này chỉ hỗ trợ một tập con JSON Schema được quy định rõ bên dưới. Không triển khai toàn bộ tiêu chuẩn JSON Schema.

## 2.2. ToolPolicy

### 2.2.1. Mô hình quyền

ToolPolicy sử dụng hai danh sách:

- **Allow list:** danh sách Tool được phép thực thi.
- **Deny list:** danh sách Tool bị từ chối thực thi.

Quy tắc:

1. Allow list mặc định rỗng.
2. Allow list rỗng đồng nghĩa không cho phép Tool nào thực thi.
3. Tool chỉ được phép thực thi nếu tên nằm trong Allow list.
4. Deny list có quyền ưu tiên cao hơn Allow list.
5. Nếu tên Tool nằm trong Deny list, Tool luôn bị từ chối, kể cả khi cũng nằm trong Allow list.
6. Tên Tool được so sánh chính xác, phân biệt chữ hoa và chữ thường.
7. Không hỗ trợ wildcard hoặc pattern matching.
8. Tool không tồn tại được xử lý bởi Registry với `ResourceNotFound`, không phải ToolPolicy.

Công thức:

```cpp
is_allowed(name) =
    name_in_allowlist(name) &&
    !name_in_denylist(name);
```

### 2.2.2. Interface

Namespace: `ogk::tools`

```cpp
class ToolPolicy {
public:
    void set_allowlist(const std::vector<std::string>& names);
    void set_denylist(const std::vector<std::string>& names);

    bool is_allowed(std::string_view tool_name) const;
};
```

Quy định:

- Hai setter thay thế toàn bộ danh sách tương ứng, không nối thêm phần tử.
- `is_allowed()` trả về `bool`.
- Không sử dụng `noexcept` cho `is_allowed()` vì việc tra cứu từ `std::string_view` có thể cần tạo đối tượng `std::string`, tùy cách triển khai container.
- Không yêu cầu ToolPolicy tự kiểm tra Tool có tồn tại trong Registry hay không.
- ToolPolicy không đảm nhiệm việc validate arguments.
- Cùng một ToolPolicy được áp dụng cho cả `ToolRegistry::execute()` và `ToolRegistry::get_definitions()`.
- `get_definitions()` chỉ cung cấp definitions của những Tool được phép theo Policy; Tool bị từ chối không được đưa vào danh sách gửi cho LLM.
- ToolPolicy không đảm nhiệm việc đăng ký hoặc gỡ Tool.

### 2.2.3. Cấu hình mặc định

Composition Root chịu trách nhiệm cấu hình Allow list.

Nếu không cấu hình Allow list, mặc định không Tool nào được phép thực thi. Đây là hành vi fail-closed nhằm tránh vô tình cho phép tất cả Tool.

### 2.2.4. Thread safety

ToolPolicy không đảm bảo thread safety.

Việc thay đổi Allow list hoặc Deny list đồng thời với hoạt động kiểm tra quyền không được hỗ trợ. Quy định này thống nhất với giới hạn thread safety của Registry tại Section 1.

---

## 2.3. JSON Schema subset

### 2.3.1. Các keyword được hỗ trợ

Validator chỉ hỗ trợ những keyword sau:

| Keyword | Mục đích |
|---|---|
| `type` | Xác định kiểu dữ liệu |
| `properties` | Khai báo schema cho các thuộc tính của object |
| `required` | Khai báo các thuộc tính bắt buộc |
| `enum` | Giới hạn các giá trị được chấp nhận |
| `additionalProperties` | Cho phép hoặc từ chối thuộc tính chưa khai báo |
| `items` | Schema áp dụng cho từng phần tử của array |
| `minimum` | Giá trị số tối thiểu |
| `maximum` | Giá trị số tối đa |
| `minLength` | Độ dài chuỗi tối thiểu |
| `maxLength` | Độ dài chuỗi tối đa |
| `minItems` | Số phần tử tối thiểu của array |
| `maxItems` | Số phần tử tối đa của array |

Mọi schema node đều phải khai báo `type`.

### 2.3.2. Các kiểu dữ liệu

Các giá trị `type` được hỗ trợ:

- `object`
- `array`
- `string`
- `integer`
- `number`
- `boolean`
- `null`

Quy tắc:

- `integer` chỉ chấp nhận giá trị JSON có kiểu integer.
- Giá trị `5.0` không được xem là `integer`, dù có giá trị toán học bằng `5`.
- `number` chấp nhận cả integer và floating-point.
- Không tự động chuyển đổi kiểu dữ liệu (type coercion).
- Không chấp nhận giá trị có kiểu khác với `type` được khai báo.

### 2.3.3. Object và properties

Ví dụ:

```json
{
  "type": "object",
  "properties": {
    "name": {
      "type": "string"
    },
    "age": {
      "type": "integer"
    }
  }
}
```

Quy định:

- `properties` phải là JSON object.
- Mỗi giá trị trong `properties` phải là một schema hợp lệ.
- Schema của từng property được validate đệ quy.
- Nếu không khai báo `properties`, object được phép chứa các key tùy ý theo quy tắc `additionalProperties`.
- Nếu object có `type: object` nhưng không có `properties`, các giá trị bên trong không bị kiểm tra kiểu theo property schema, nhưng vẫn phải được duyệt để kiểm tra UTF-8 và giới hạn độ sâu.

### 2.3.4. Required

Ví dụ:

```json
{
  "type": "object",
  "properties": {
    "name": {
      "type": "string"
    },
    "age": {
      "type": "integer"
    }
  },
  "required": ["name"]
}
```

Quy định:

- `required` phải là JSON array.
- Mỗi phần tử phải là string.
- Các tên trong `required` phải duy nhất.
- `required: []` là hợp lệ và tương đương không có thuộc tính bắt buộc.
- **Mọi tên trong `required` bắt buộc phải tồn tại trong `properties`.**
- Quy tắc này áp dụng bất kể `additionalProperties` là `true`, `false` hay không được khai báo.
- Nếu arguments thiếu một thuộc tính bắt buộc, Validator trả về lỗi tại JSON Pointer của thuộc tính bị thiếu.
- Nếu nhiều thuộc tính bắt buộc cùng thiếu, kiểm tra theo thứ tự tên tăng dần byte-wise để đảm bảo kết quả xác định.

Ví dụ không hợp lệ:

```json
{
  "type": "object",
  "properties": {
    "name": {
      "type": "string"
    }
  },
  "required": ["name", "age"]
}
```

Lý do: `age` không được khai báo trong `properties`.

### 2.3.5. AdditionalProperties

`additionalProperties` chỉ nhận giá trị boolean.

Quy định:

- Mặc định là `true` nếu không khai báo.
- `true`: cho phép các thuộc tính không được khai báo trong `properties`.
- `false`: từ chối mọi thuộc tính không được khai báo trong `properties`.
- Giá trị khác boolean là schema không hợp lệ.
- Không hỗ trợ `additionalProperties` dưới dạng schema object.

Trường hợp không có `properties`:

- `additionalProperties: true` hoặc không khai báo: cho phép key tùy ý.
- `additionalProperties: false`: chỉ cho phép object rỗng.

Ví dụ:

```json
{
  "type": "object",
  "properties": {
    "name": {
      "type": "string"
    }
  },
  "additionalProperties": false
}
```

Arguments có key khác `name` sẽ bị từ chối.

### 2.3.6. Array và items

Ví dụ:

```json
{
  "type": "array",
  "items": {
    "type": "string"
  },
  "minItems": 1,
  "maxItems": 10
}
```

Quy định:

- `items` là tùy chọn.
- Nếu có `items`, giá trị phải là schema hợp lệ và được validate đệ quy.
- Nếu không có `items`, phần tử array không bị giới hạn kiểu dữ liệu riêng, nhưng vẫn phải tuân thủ kiểm tra UTF-8 và độ sâu.
- `minItems` là tùy chọn, mặc định bằng `0`.
- **Mọi schema node có `type: array` bắt buộc phải khai báo `maxItems`.**
- Quy tắc bắt buộc `maxItems` áp dụng đệ quy, bao gồm cả array nằm bên trong `items`, `properties` hoặc các schema node lồng nhau khác.
- `minItems` và `maxItems` phải là số nguyên không âm.
- Nếu `minItems > maxItems`, schema không hợp lệ.
- Nếu số phần tử arguments nhỏ hơn `minItems` hoặc lớn hơn `maxItems`, Validator trả về lỗi.

Giá trị `maxItems` cụ thể của từng Tool được quy định trong tài liệu `phase-3-tool-specs.md`.

### 2.3.7. Number và minimum/maximum

Ví dụ:

```json
{
  "type": "number",
  "minimum": 0,
  "maximum": 100
}
```

Quy định:

- `minimum` và `maximum` là tùy chọn.
- Nếu được khai báo, giá trị phải là JSON number.
- Giá trị boolean không được chấp nhận dù trong C++ có thể có những cách biểu diễn liên quan đến số.
- `minimum` và `maximum` được áp dụng cho cả `integer` và `number`.
- Hai giới hạn đều inclusive.
- Nếu `minimum > maximum`, schema không hợp lệ.
- Không ép toàn bộ giá trị sang `double` hoặc `int64_t` theo cách có thể làm mất độ chính xác hoặc gây overflow.
- So sánh phải an toàn đối với signed integer, unsigned integer và floating-point.

### 2.3.8. String và độ dài

Ví dụ:

```json
{
  "type": "string",
  "minLength": 1,
  "maxLength": 100
}
```

Quy định:

- `minLength` và `maxLength` là tùy chọn.
- Giá trị phải là số nguyên không âm.
- Nếu `minLength > maxLength`, schema không hợp lệ.
- Độ dài chuỗi được tính theo Unicode code points, không tính theo số byte UTF-8.
- Chuỗi phải là UTF-8 hợp lệ.
- Nếu chuỗi không thỏa mãn giới hạn, Validator trả về lỗi tương ứng.

### 2.3.9. Enum

Ví dụ:

```json
{
  "type": "string",
  "enum": ["read", "write", "delete"]
}
```

Quy định:

- `enum` phải là JSON array.
- `enum` không được rỗng.
- Mọi giá trị trong enum phải phù hợp với `type` của schema.
- Không cho phép giá trị enum có kiểu không tương thích.
- So sánh enum sử dụng deep equality có xét kiểu dữ liệu, không sử dụng trực tiếp `nlohmann::json::operator==` làm quy tắc so sánh của Validator.

Quy tắc so sánh:

1. Phải so sánh kiểu dữ liệu trước.
2. Signed integer và unsigned integer được xem là cùng nhóm integer; so sánh giá trị chính xác, không để xảy ra overflow.
3. Floating-point chỉ so sánh với floating-point.
4. Integer không bằng floating-point, kể cả khi có cùng giá trị toán học.
5. Object và array được so sánh đệ quy theo quy tắc trên.
6. Các kiểu dữ liệu khác chỉ bằng nhau khi cùng kiểu và có cùng giá trị.

Ví dụ:

| Schema type | Enum | Input | Kết quả |
|---|---|---|---|
| `integer` | `[1, 2]` | `1` | Hợp lệ |
| `integer` | `[1, 2]` | `1.0` | Không hợp lệ |
| `number` | `[1, 2]` | `1` | Hợp lệ |
| `number` | `[1, 2]` | `1.0` | Không hợp lệ |
| `number` | `[1.0, 2.0]` | `1.0` | Hợp lệ |
| `number` | `[1, 1.0]` | `1` | Hợp lệ |
| `number` | `[1, 1.0]` | `1.0` | Hợp lệ |

**Hướng dẫn khi viết Tool schema:** nếu muốn enum của kiểu `number` chấp nhận một giá trị floating-point như `1.0`, phải khai báo rõ giá trị float trong enum. Ví dụ: `[1.0, 2.0]`. Không giả định rằng enum `[1, 2]` sẽ tự động chấp nhận `1.0` hoặc `2.0`.

---

## 2.4. UTF-8 validation

Validator phải kiểm tra UTF-8 hợp lệ đối với:

- Mọi JSON string value.
- Mọi key của JSON object.
- Các node lồng nhau trong object và array.
- Các giá trị và key bên trong schema.
- Các giá trị và key bên trong arguments, kể cả object không có `properties`.

Quy tắc kiểm tra tại node hiện tại:

1. Nếu node hiện tại là string, kiểm tra UTF-8 của chính string value đó.
2. Nếu node hiện tại là object, kiểm tra UTF-8 của các key thuộc object đó theo thứ tự tăng dần byte-wise.
3. Sau khi hoàn thành kiểm tra tại node hiện tại, tiếp tục duyệt các node con theo thứ tự xác định: object theo key tăng dần byte-wise, array theo index tăng dần.
4. Không kiểm tra toàn bộ cây con của một object ngay tại node cha. Mỗi node con được kiểm tra khi Validator duyệt đến node đó.

Thứ tự này giúp các implementation trả về cùng một lỗi khi input có nhiều lỗi UTF-8 hoặc nhiều lỗi validation.

Kết quả:

- UTF-8 không hợp lệ trong arguments: trả về `InvalidArguments`.
- UTF-8 không hợp lệ trong schema: ném `ToolConfigError`.

Nếu JSON parser phát hiện UTF-8 không hợp lệ trước khi Validator được gọi, lỗi parse được xử lý tại tầng thực hiện parse tương ứng.

---

## 2.5. Độ sâu tối đa

Validator giới hạn độ sâu nhằm tránh schema hoặc arguments có cấu trúc lồng nhau quá lớn.

| Loại dữ liệu | Độ sâu tối đa |
|---|---:|
| Schema | 16 |
| Arguments | 32 |

### 2.5.1. Cách tính độ sâu

Độ sâu được tính trên **cấu trúc JSON thực tế**, không tính theo số tầng logic của Tool.

Quy tắc:

- Root JSON value có depth `0`.
- Mỗi lần đi qua một object member hoặc một array element để đến value con, depth tăng `1`.
- Key của object không được tính như một JSON value độc lập.
- Schema cũng được tính theo cấu trúc JSON thực tế của schema document, bao gồm các object trung gian như `properties`, tên property, `items` và các node schema lồng nhau.

Ví dụ về đường dẫn cấu trúc trong schema:

```text
root
 └── properties
      └── name
           └── items
                └── properties
```

Mỗi value con trên đường dẫn làm depth tăng một cấp.

### 2.5.2. Hành vi khi vượt giới hạn

- Schema có depth lớn hơn `16`: ném `ToolConfigError`.
- Arguments có depth lớn hơn `32`: trả về `InvalidArguments`.
- Depth đúng bằng giới hạn được chấp nhận.
- Việc kiểm tra độ sâu áp dụng cả với object không có `properties` hoặc array không có `items`.
- Không bỏ qua việc duyệt node con chỉ vì node cha không khai báo schema cho các node đó.

---

## 2.6. Thứ tự validation và tính xác định

Validator chỉ trả về **lỗi đầu tiên** tìm được.

Để đảm bảo các implementation có kết quả nhất quán, thứ tự validation được cố định như sau.

### 2.6.1. Tại mỗi node

1. Kiểm tra độ sâu.
2. Kiểm tra UTF-8 tại node hiện tại:
   - String value của chính node đó.
   - Các key của object hiện tại.
3. Kiểm tra `type`.
4. Kiểm tra `enum`.
5. Kiểm tra các ràng buộc scalar:
   - String: `minLength`, sau đó `maxLength`.
   - Number: `minimum`, sau đó `maximum`.
   - Array: `minItems`, sau đó `maxItems`.

### 2.6.2. Riêng với object

Sau các bước chung:

1. Kiểm tra `required` theo thứ tự tên tăng dần byte-wise.
2. Kiểm tra `additionalProperties` theo thứ tự key tăng dần byte-wise.
3. Kiểm tra `properties` theo thứ tự key tăng dần byte-wise.

### 2.6.3. Riêng với array

Sau các bước chung:

- Kiểm tra từng phần tử theo thứ tự index tăng dần, bắt đầu từ `0`.
- Nếu có `items`, validate từng phần tử bằng schema trong `items`.
- Nếu không có `items`, vẫn duyệt các phần tử để kiểm tra độ sâu và UTF-8.

Thứ tự validation trên là hợp đồng bắt buộc, không phải gợi ý triển khai.

---

## 2.7. Validation error và field_path

Kết quả validation sử dụng `ValidationError` với thông tin cần thiết để Registry tạo `ToolResult` theo Section 1.

`field_path` sử dụng JSON Pointer theo RFC 6901.

Quy tắc:

- Root arguments object có path là chuỗi rỗng `""`.
- Mỗi object member thêm một path segment.
- Mỗi array element thêm index của phần tử.
- Ký tự `~` trong key được escape thành `~0`.
- Ký tự `/` trong key được escape thành `~1`.

Ví dụ:

| Vị trí lỗi | field_path |
|---|---|
| Root | `""` |
| `name` | `/name` |
| `user/name` | `/user~1name` |
| `user~name` | `/user~0name` |
| Phần tử thứ hai của `items` | `/items/1` |

Với lỗi thiếu required property:

- `field_path` trỏ đến property bị thiếu.
- Message phải nêu rõ tên property.
- Ví dụ: `Missing required field 'name'`.

Với lỗi không xác định được một field cụ thể, có thể sử dụng root path `""`.

---

## 2.8. Schema configuration errors

Schema không hợp lệ là lỗi cấu hình Tool, không phải lỗi arguments do LLM gửi lên.

Các trường hợp sau phải ném `ToolConfigError`:

1. Schema root không phải JSON object.
2. Schema node không có `type`.
3. `type` không thuộc danh sách được hỗ trợ.
4. Có keyword không được hỗ trợ.
5. Keyword có kiểu dữ liệu không hợp lệ.
6. `properties` không phải object.
7. Schema con trong `properties` không hợp lệ.
8. `required` không phải array.
9. Phần tử trong `required` không phải string.
10. `required` chứa tên trùng lặp.
11. `required` tham chiếu property không được khai báo trong `properties`.
12. `additionalProperties` không phải boolean.
13. `items` không phải schema hợp lệ.
14. Array schema không khai báo `maxItems`.
15. `minItems` hoặc `maxItems` không phải số nguyên không âm.
16. `minItems > maxItems`.
17. `minimum` hoặc `maximum` không phải JSON number.
18. `minimum > maximum`.
19. `minLength` hoặc `maxLength` không phải số nguyên không âm.
20. `minLength > maxLength`.
21. `enum` không phải array hoặc là array rỗng.
22. Giá trị trong enum không phù hợp với `type`.
23. Schema vượt giới hạn độ sâu.
24. Schema chứa UTF-8 không hợp lệ.

`ToolConfigError` được định nghĩa tại `tool_errors.hpp` và dùng thống nhất giữa Registry và JsonSchemaValidator.

Không tạo nhiều loại exception riêng cho từng lỗi schema trong phạm vi Section này.

---

## 2.9. Arguments validation errors

Các trường hợp sau được xem là lỗi arguments:

- Arguments không đúng kiểu được khai báo.
- Thiếu required property.
- Có property không được phép.
- Giá trị không nằm trong enum.
- Giá trị số nằm ngoài minimum/maximum.
- String không thỏa mãn minLength/maxLength.
- Array không thỏa mãn minItems/maxItems.
- Phần tử array không đúng schema trong items.
- Arguments vượt giới hạn độ sâu.
- Arguments chứa UTF-8 không hợp lệ.

Các lỗi này không ném `ToolConfigError`.

Validator trả về `ValidationError`; Registry chuyển lỗi thành `ToolResult` với `ErrorCode::InvalidArguments` theo Section 1.

---

## 2.10. JsonSchemaValidator interface

Namespace: `ogk::tools`

```cpp
class JsonSchemaValidator {
public:
    void validate_schema(const nlohmann::json& schema) const;

    std::optional<ValidationError> validate(
        const nlohmann::json& arguments,
        const nlohmann::json& schema
    ) const;
};
```

Quy định:

### `validate_schema()`

- Kiểm tra cấu trúc và tính hợp lệ của schema.
- Không trả về `ValidationError`.
- Nếu schema không hợp lệ, ném `ToolConfigError`.
- Schema hợp lệ mới được sử dụng trong quá trình validate arguments.

### `validate()`

- Nhận arguments và schema đã được validate trước đó.
- Nếu arguments hợp lệ, trả về `std::nullopt`.
- Nếu arguments không hợp lệ, trả về `ValidationError` đầu tiên.
- Nếu schema chưa được validate hoặc không hợp lệ, ném `ToolConfigError`.
- Không tự động sửa hoặc chuyển đổi arguments.
- Không thực hiện type coercion.

Registry chịu trách nhiệm cache schema và trạng thái schema đã được validate của từng Tool. Không validate lại schema ở mỗi lần thực thi nếu schema không thay đổi.

---

## 2.11. Registry execution flow

Thứ tự thực thi Tool bắt buộc:

```text
ToolRegistry::execute()
        |
        v
   Lookup Tool
        |
        |-- Không tồn tại
        |       |
        |       v
        |  ResourceNotFound
        |
        v
  Check ToolPolicy
        |
        |-- Không được phép
        |       |
        |       v
        |  PermissionDenied
        |
        v
 Validate arguments
        |
        |-- Không hợp lệ
        |       |
        |       v
        |  InvalidArguments
        |
        v
  Tool::execute()
        |
        |-- Expected business error
        |       |
        |       v
        |  ToolResult::make_error()
        |
        |-- Unexpected exception
        |       |
        |       v
        |  Registry catches exception
        |       |
        |       v
        |  InternalError
        |
        v
   Return ToolResult
```

Quy định:

- Lookup Tool trước khi kiểm tra Policy.
- Tool không tồn tại trả về `ResourceNotFound`.
- Tool bị Policy từ chối trả về `PermissionDenied`.
- Arguments không hợp lệ trả về `InvalidArguments`.
- Expected business errors được Tool biểu diễn bằng `ToolResult`.
- Unexpected exceptions được Registry bắt và chuyển thành `InternalError`.
- Registry ghi log thông tin kỹ thuật cần thiết.
- Không đưa trực tiếp `e.what()` hoặc thông tin exception nội bộ cho LLM.

`ToolConfigError` là lỗi cấu hình, không phải lỗi arguments trong một lần gọi Tool. Lỗi này được phát hiện khi đăng ký hoặc validate cấu hình Tool và được xử lý theo hợp đồng exception của Registry.

---

## 2.12. Giới hạn và các tính năng không hỗ trợ

Section này không triển khai toàn bộ JSON Schema.

Các tính năng không hỗ trợ:

- `oneOf`
- `anyOf`
- `allOf`
- `not`
- `if`
- `then`
- `else`
- `pattern`
- `format`
- `uniqueItems`
- `minProperties`
- `maxProperties`
- `additionalProperties` dưới dạng schema object
- Wildcard trong ToolPolicy
- Type coercion
- Tự động sửa arguments không hợp lệ

Các điều kiện nghiệp vụ phức tạp, chẳng hạn một field chỉ bắt buộc khi một field khác có giá trị cụ thể, phải được kiểm tra trong `Tool::execute()` hoặc tầng xử lý nghiệp vụ tương ứng. Không mở rộng JSON Schema subset chỉ để xử lý những trường hợp này.

---

## 2.13. Test contracts

Các test dưới đây là hợp đồng hành vi bắt buộc. Implementation phải đáp ứng đầy đủ trước khi xem Section 2 hoàn thành.

### 2.13.1. ToolPolicy tests

| ID | Nội dung | Kết quả mong đợi |
|---|---|---|
| TP-T01 | Allow list mặc định rỗng | Tất cả Tool bị từ chối |
| TP-T02 | Tool nằm trong Allow list | Được phép |
| TP-T03 | Tool không nằm trong Allow list | Bị từ chối |
| TP-T04 | Tool nằm trong Deny list và Allow list | Bị từ chối |
| TP-T05 | So sánh tên phân biệt hoa thường | Chỉ tên khớp chính xác được chấp nhận |
| TP-T06 | Setter được gọi nhiều lần | Danh sách được thay thế hoàn toàn |
| TP-T07 | Tên Tool không tồn tại | Policy không tự xử lý; Registry trả `ResourceNotFound` |

### 2.13.2. JSON Schema Validator tests

| ID | Nội dung | Kết quả mong đợi |
|---|---|---|
| JS-T01 | Validate object hợp lệ | Thành công |
| JS-T02 | Validate string đúng type | Thành công |
| JS-T03 | Validate integer đúng type | Thành công |
| JS-T04 | Integer nhận input floating-point `5.0` | `InvalidArguments` |
| JS-T05 | Number nhận integer và floating-point | Thành công |
| JS-T06 | Validate boolean và null | Thành công |
| JS-T07 | Thiếu required property | `InvalidArguments`, đúng field_path |
| JS-T08 | Required chứa tên trùng lặp | `ToolConfigError` |
| JS-T09 | Required chứa phần tử không phải string | `ToolConfigError` |
| JS-T10 | Required tham chiếu property không khai báo | `ToolConfigError`, kể cả khi `additionalProperties: true` |
| JS-T11 | Required rỗng `[]` | Schema hợp lệ |
| JS-T12 | `additionalProperties: false`, input có key ngoài properties | `InvalidArguments` |
| JS-T13 | `additionalProperties: true`, input có key ngoài properties | Thành công nếu các điều kiện khác hợp lệ |
| JS-T14 | `additionalProperties` không phải boolean | `ToolConfigError` |
| JS-T15 | Object không có properties và `additionalProperties: true` | Chấp nhận key tùy ý, vẫn kiểm tra UTF-8 và depth |
| JS-T16 | Object không có properties và `additionalProperties: false` | Chỉ chấp nhận object rỗng |
| JS-T17 | Array có items hợp lệ | Thành công |
| JS-T18 | Array không khai báo maxItems | `ToolConfigError` |
| JS-T19 | Array lồng nhau thiếu maxItems ở node con | `ToolConfigError` |
| JS-T20 | Array vượt maxItems | `InvalidArguments` |
| JS-T21 | Array nhỏ hơn minItems | `InvalidArguments` |
| JS-T22 | `minItems > maxItems` | `ToolConfigError` |
| JS-T23 | String vượt maxLength | `InvalidArguments` |
| JS-T24 | String nhỏ hơn minLength | `InvalidArguments` |
| JS-T25 | String Unicode hợp lệ, độ dài tính theo code points | Kết quả đúng theo Unicode code points |
| JS-T26 | Number nhỏ hơn minimum hoặc lớn hơn maximum | `InvalidArguments` |
| JS-T27 | minimum hoặc maximum không phải JSON number | `ToolConfigError` |
| JS-T28 | minimum lớn hơn maximum | `ToolConfigError` |
| JS-T29 | Enum string khớp chính xác | Thành công |
| JS-T30 | Enum phân biệt integer và floating-point | Kết quả theo quy tắc type-aware equality |
| JS-T31 | Enum integer/unsigned integer lớn, không overflow khi so sánh | So sánh chính xác |
| JS-T32 | Enum object/array lồng nhau | Deep equality đúng quy tắc |
| JS-T33 | Enum rỗng hoặc không phải array | `ToolConfigError` |
| JS-T34 | Enum có giá trị không tương thích type | `ToolConfigError` |
| JS-T35 | JSON Pointer có key chứa `/` và `~` | Escape đúng RFC 6901 |
| JS-T36 | Nhiều lỗi cùng xuất hiện | Trả về lỗi đầu tiên theo thứ tự quy định tại Section 2.6 |
| JS-T37 | Schema chưa được validate trước khi gọi validate | Ném `ToolConfigError` |
| JS-T38 | Schema vượt depth 16 | `ToolConfigError` |
| JS-T39 | Schema có depth đúng 16 | Được chấp nhận nếu các điều kiện khác hợp lệ |
| JS-T40 | Schema có depth 17 | `ToolConfigError` |
| JS-T41 | Arguments vượt depth 32 | `InvalidArguments` |
| JS-T42 | Arguments có depth đúng 32 | Được chấp nhận nếu các điều kiện khác hợp lệ |
| JS-T43 | Arguments có depth 33 | `InvalidArguments` |
| JS-T44 | Arguments chứa string UTF-8 không hợp lệ | `InvalidArguments` |
| JS-T45 | Arguments chứa object key UTF-8 không hợp lệ | `InvalidArguments` |
| JS-T46 | Schema chứa string hoặc object key UTF-8 không hợp lệ | `ToolConfigError` |
| JS-T47 | Schema có keyword không hỗ trợ | `ToolConfigError` |
| JS-T48 | Schema node không có type | `ToolConfigError` |
| JS-T49 | Schema root không phải object | `ToolConfigError` |
| JS-T50 | `minLength` hoặc `maxLength` không phải số nguyên không âm | `ToolConfigError` |
| JS-T51 | `minLength > maxLength` | `ToolConfigError` |
| JS-T52 | `minItems` hoặc `maxItems` không phải số nguyên không âm | `ToolConfigError` |
| JS-T53 | Schema có nested object/array không khai báo schema con nhưng vẫn vượt depth | Vẫn phát hiện lỗi depth |
| JS-T54 | Kiểm tra thứ tự UTF-8 tại node hiện tại và các node con | Kết quả theo thứ tự quy định tại Section 2.4 và 2.6 |

---

## 2.14. Decisions Register

Các quyết định trong Section 2 đã được phê duyệt và được ánh xạ vào Decisions Register chung của Phase 3.

### 2.14.1. ToolPolicy decisions

| P3-ID | Local ID | Decision | Status |
|---|---|---|---|
| P3-D26 | TP-01 | ToolPolicy sử dụng Allow list và Deny list | Approved |
| P3-D27 | TP-02 | Allow list mặc định rỗng, mặc định deny all | Approved |
| P3-D28 | TP-03 | Deny list có quyền ưu tiên cao hơn Allow list | Approved |
| P3-D29 | TP-04 | Tool name được so sánh chính xác, phân biệt hoa thường; không wildcard | Approved |
| P3-D30 | TP-05 | ToolPolicy không xử lý Tool không tồn tại | Approved |
| P3-D31 | TP-06 | Setter thay thế toàn bộ danh sách tương ứng | Approved |
| P3-D32 | TP-07 | Policy dùng chung cho execute và get_definitions; Composition Root cấu hình Allow list | Approved |

### 2.14.2. JSON Schema decisions

| P3-ID | Local ID | Decision | Status |
|---|---|---|---|
| P3-D33 | JS-01 | Chỉ hỗ trợ tập con JSON Schema được liệt kê tại Section 2.3 | Approved |
| P3-D34 | JS-02 | Mọi schema node bắt buộc có type; hỗ trợ object, array, string, integer, number, boolean, null | Approved |
| P3-D35 | JS-03 | Object không có properties được phép nhận key tùy theo additionalProperties; vẫn kiểm tra UTF-8 và depth | Approved |
| P3-D36 | JS-04 | Mọi array schema bắt buộc khai báo maxItems, áp dụng đệ quy | Approved |
| P3-D37 | JS-05 | minItems mặc định 0; minItems và maxItems phải hợp lệ và minItems không vượt maxItems | Approved |
| P3-D38 | JS-06 | minimum và maximum inclusive, áp dụng cho integer và number; so sánh phải tránh mất chính xác và overflow | Approved |
| P3-D39 | JS-07 | String length tính theo Unicode code points | Approved |
| P3-D40 | JS-08 | Arguments và schema phải được kiểm tra UTF-8 ở mọi string value và object key | Approved |
| P3-D41 | JS-09 | Enum sử dụng deep equality có xét kiểu dữ liệu; integer và float không tự động bằng nhau | Approved |
| P3-D42 | JS-10 | Validator chỉ trả lỗi đầu tiên theo thứ tự validation cố định | Approved |
| P3-D43 | JS-11 | field_path sử dụng JSON Pointer RFC 6901 | Approved |
| P3-D44 | JS-12 | Schema configuration errors dùng ToolConfigError thống nhất | Approved |
| P3-D45 | JS-13 | Schema depth tối đa 16; arguments depth tối đa 32; root depth 0, tính theo cấu trúc JSON | Approved |
| P3-D46 | JS-14 | validate() chỉ nhận schema đã được validate; Registry chịu trách nhiệm cache schema và trạng thái validate | Approved |
| P3-D47 | JS-15 | Unsupported keywords và các tính năng ngoài subset không được triển khai | Approved |
| P3-D48 | JS-16 | Required phải là danh sách string duy nhất và mọi tên đều phải khai báo trong properties | Approved |
| P3-D49 | JS-17 | additionalProperties chỉ nhận boolean; mặc định true; false từ chối key không khai báo | Approved |

---

## 2.15. Cross-document synchronization

Các quyết định của Section 2 phải được đồng bộ với tài liệu Phase 3 Overall Design và Tool Specs để tránh mâu thuẫn giữa thiết kế tổng thể và interface specification.

### 2.15.1. Overall Design – Section 3.7

Cập nhật phần JSON Schema subset trong Overall Design:

- Bổ sung `minItems`.
- Bổ sung `maxItems`.
- Ghi rõ mọi array schema node phải khai báo `maxItems`, kể cả array lồng nhau.
- Ghi rõ `required` chỉ được tham chiếu property đã khai báo trong `properties`.
- Ghi rõ `additionalProperties` chỉ nhận boolean và mặc định là `true`.
- Ghi rõ `minimum` và `maximum` phải là JSON number.
- Ghi rõ enum sử dụng type-aware deep equality; integer và floating-point được phân biệt.
- Bổ sung quy tắc UTF-8 cho mọi string value và object key, bao gồm node lồng nhau.
- Bổ sung giới hạn depth: schema 16, arguments 32; root depth 0; tính theo cấu trúc JSON thực tế.
- Bổ sung thứ tự validation xác định tại Section 2.6.
- Ghi rõ schema configuration errors sử dụng `ToolConfigError`.

### 2.15.2. Overall Design – Error handling

Đồng bộ danh sách ErrorCode và exception handling:

- `InvalidArguments`: arguments không hợp lệ.
- `PermissionDenied`: Tool bị Policy từ chối.
- `ResourceNotFound`: Tool không tồn tại.
- `ResourceUnavailable`: tài nguyên hoặc dịch vụ cần thiết không khả dụng.
- `ExecutionFailed`: Tool thực thi thất bại theo nghiệp vụ.
- `Timeout`: thao tác vượt quá thời gian cho phép.
- `InternalError`: lỗi nội bộ không dự kiến.
- `InteractionUnavailable`: không có cơ chế tương tác người dùng khả dụng.

`ToolConfigError` là exception phục vụ lỗi cấu hình Tool/schema, không phải một `ErrorCode` của ToolResult.

### 2.15.3. Tool Specifications – `phase-3-tool-specs.md`

Mỗi Tool có schema chứa array phải khai báo `maxItems` cụ thể.

Các giá trị khởi điểm đã được chốt:

| Tool / Field | maxItems |
|---|---:|
| `exec.arguments` | 64 |
| `tags` | 20 |
| `clarify.options` | 10 |

Quy định:

- Các giá trị trên phải được ghi trực tiếp vào schema của Tool tương ứng trong `phase-3-tool-specs.md`.
- Nếu Tool có array lồng nhau, từng array schema node phải khai báo `maxItems` riêng.
- Không được dựa vào một giới hạn ở array cha để ngầm giới hạn array con.
- Những Tool khác có array field phải xác định giới hạn cụ thể trong tài liệu Tool Specs trước khi triển khai.
- Không được để schema của Tool vi phạm quy tắc bắt buộc `maxItems` tại Section 2.3.6.

### 2.15.4. Global Decisions Register

- Các quyết định TP-01..TP-07 được ánh xạ thành P3-D26..P3-D32.
- Các quyết định JS-01..JS-17 được ánh xạ thành P3-D33..P3-D49.
- Trạng thái của toàn bộ quyết định trong Section 2 là `Approved`.
- Khi tổng hợp Decisions Register chung, sử dụng P3-ID; local ID chỉ giữ lại để truy vết về Section 2.

---

**End of Section 2 – ToolPolicy & JSON Schema Validator**

