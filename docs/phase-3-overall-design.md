# OGK-Agent
# PHASE 3 — OVERALL SYSTEM DESIGN

**Phiên bản:** 1.2  
**Trạng thái:** Design Draft — Chờ khóa thiết kế  
**Ngôn ngữ triển khai:** C++23  
**Backend:** Ollama API  
**Repository:** https://github.com/tuanholmestar/OGK-Agent

---

## 1. Mục đích tài liệu

Tài liệu này mô tả thiết kế tổng thể Phase 3 của OGK-Agent, bao gồm:

- Tool System.
- ToolRegistry và ToolPolicy.
- Environment.
- Infrastructure dùng chung.
- Skill System.
- Cấu trúc thư mục.
- Chiến lược kiểm thử.
- Các quyết định thiết kế và trạng thái phê duyệt.

Đây là tài liệu thiết kế, không phải tài liệu hướng dẫn sử dụng hay đặc tả chi tiết từng API.

Đặc tả chi tiết được tách theo phạm vi:

- `phase-3-interface-spec.md`: Core Types, ToolPolicy và JSON Schema Validator.
- `phase-3-tool-specs.md`: arguments, JSON Schema, ToolResult, ErrorCode, validation và test contract cho 10 Tool operations.
- `phase-3-api-contracts.md`: API contracts của ToolRegistry, Environment, HttpClient, MemoryStore và UserInteraction.

### Nguyên tắc

1. Tuân thủ Architecture v1.0 đã đóng băng và các interface Phase 2 đã đóng băng.
2. Không thay đổi public interface Phase 2 nếu không có phê duyệt.
3. Không để Tool phụ thuộc AgentLoop.
4. Không để Skill System phụ thuộc trực tiếp AgentLoop.
5. Tách biệt business logic và infrastructure.
6. Ưu tiên YAGNI, RAII và smart pointer.
7. Mọi quyết định phải có trạng thái và nguồn xác định.

---

## 2. Phạm vi Phase 3

### 2.1. Tool System

Xây dựng hệ thống cho phép:

- Đăng ký Tool tại runtime.
- Tra cứu Tool theo tên.
- Quản lý vòng đời Tool.
- Kiểm soát quyền truy cập.
- Kiểm tra arguments trước khi thực thi.
- Chuẩn hóa kết quả và lỗi.

### 2.2. Skill System

Xây dựng hệ thống cho phép:

- Scan thư mục `skills/`.
- Đọc nội dung Markdown.
- Phân tích metadata.
- Chọn Skill theo task.
- Hỗ trợ nhiều Skill.
- Chuẩn bị nội dung để AgentLoop inject vào system prompt.

### 2.3. Environment

Cung cấp abstraction cho thao tác với hệ thống:

- Đọc file.
- Ghi file.
- Đọc dữ liệu nhị phân nội bộ.
- Kiểm tra quyền truy cập đường dẫn.
- Thực thi process.

Phase 3 triển khai NativeEnvironment ở phạm vi tối thiểu. SandboxEnvironment chỉ được thiết kế cho khả năng mở rộng, không tuyên bố đã triển khai sandbox hoàn chỉnh.

---

## 3. Tool System

### 3.1. Tool Interface

Mọi Tool tuân theo một interface thống nhất, gồm:

- `name()`: trả về tên duy nhất.
- `description()`: mô tả chức năng.
- `parameters()`: cung cấp JSON Schema.
- `execute(arguments)`: thực thi và trả về ToolResult.

Tool không được biết AgentLoop hoặc Harness tồn tại.

### 3.2. ToolResult

ToolResult biểu diễn kết quả thực thi theo cấu trúc thống nhất.

**Success:**
- `success()` là `true`; `error()` là `nullopt`.
- `data()` luôn là JSON object. Tool không có dữ liệu trả về dùng `{}`; scalar hoặc array phải được đặt trong một object có field phù hợp.

**Error:**
- `success()` là `false`; `data()` là JSON null và `error()` chứa ErrorInfo.
- ErrorInfo chứa ErrorCode, message dành cho tầng gọi Tool và `field_path` nếu lỗi liên quan đến input cụ thể.
- `field_path` sử dụng JSON Pointer RFC 6901, với root arguments là `""`.

Các ErrorCode runtime:

| Code | Ý nghĩa |
|---|---|
| `InvalidArguments` (`INVALID_ARGUMENTS`) | Arguments không hợp lệ |
| `PermissionDenied` (`PERMISSION_DENIED`) | Không được phép thực hiện |
| `ResourceNotFound` (`RESOURCE_NOT_FOUND`) | Không tìm thấy Tool hoặc tài nguyên |
| `ResourceUnavailable` (`RESOURCE_UNAVAILABLE`) | Tài nguyên hoặc dịch vụ tạm thời không khả dụng |
| `ExecutionFailed` (`EXECUTION_FAILED`) | Thực thi thất bại |
| `Timeout` (`TIMEOUT`) | Quá thời gian cho phép |
| `InternalError` (`INTERNAL_ERROR`) | Lỗi nội bộ không dự kiến |
| `InteractionUnavailable` (`INTERACTION_UNAVAILABLE`) | Không có kênh tương tác người dùng |

`ToolConfigError` là exception cấu hình cho schema/Tool không hợp lệ, không phải ErrorCode và không phải lỗi arguments runtime. Registry phát hiện lỗi này khi đăng ký hoặc xác thực cấu hình Tool. Exception không dự kiến trong quá trình thực thi được Registry ghi log và chuyển thành `InternalError`. Không đưa raw exception details, bao gồm `e.what()` hoặc thông tin kỹ thuật nhạy cảm, trực tiếp cho LLM.

### 3.3. Danh sách Tool

| Tool | Implementation | Chức năng |
|---|---|---|
| `exec` | ExecTool | Thực thi process |
| `read_file` | FileTool (Read) | Đọc file văn bản |
| `write_file` | FileTool (Write) | Ghi file văn bản |
| `web_search` | WebSearchTool | Tìm kiếm thông tin |
| `memory_save` | MemoryTool (Save) | Lưu memory |
| `memory_search` | MemoryTool (Search) | Truy vấn memory |
| `calculator` | CalculatorTool | Tính biểu thức |
| `todo_list` | TodoListTool | Quản lý danh sách công việc |
| `vision_analyze` | VisionAnalyzeTool | Phân tích hình ảnh |
| `clarify` | ClarifyTool | Yêu cầu người dùng làm rõ |

Có 10 Tool operations và 8 implementation classes.

FileTool và MemoryTool có thể dùng chung implementation class nhưng phải được khởi tạo thành các instance riêng, mỗi instance đại diện cho một operation và một tên Tool duy nhất.

### 3.4. ToolRegistry

ToolRegistry chịu trách nhiệm:

- Runtime registration.
- Lookup theo tên.
- Quản lý ownership.
- Factory/Registry Pattern.
- Liệt kê Tool.
- Lấy danh sách ToolDefinition.
- Thực thi Tool theo tên.

Registry sở hữu Tool bằng `std::unique_ptr`.

Nếu unregister được human approval trong tương lai, pointer/reference lifetime sau thao tác đó cần được xác định trước khi triển khai. Hiện tại chưa có public unregister API được chốt; các module phía trên nên thao tác theo tên Tool thay vì lưu raw pointer lâu dài.

Lỗi cấu hình Registry, chẳng hạn đăng ký trùng tên hoặc factory không hợp lệ, được xử lý bằng exception.

Lỗi trong quá trình chạy Tool được chuyển thành ToolResult.

Schema của Tool được xác thực khi đăng ký; schema không hợp lệ là lỗi cấu hình và phải ném `ToolConfigError`. Registry cache schema đã xác thực và trạng thái xác thực đó, không xác thực lại ở mỗi lần execute nếu schema không đổi.

Unregister vẫn là nội dung Proposed, chưa được quyết định và không phải yêu cầu bắt buộc của MVP. Chưa xác định public API hoặc lifecycle behavior cho chức năng này; cần human approval trước khi đưa vào implementation.

### 3.5. ToolPolicy

ToolPolicy quản lý quyền truy cập theo tên Tool.

**Quy tắc chính thức:**

1. Có Allow list và Deny list.
2. Deny-over-Allow: Deny luôn ưu tiên hơn Allow.
3. Allow list mặc định rỗng.
4. Tool không nằm trong Allow list bị từ chối.
5. Tool nằm trong Deny list luôn bị từ chối, kể cả khi cũng nằm trong Allow list.
6. So sánh tên chính xác, phân biệt chữ hoa/chữ thường; không hỗ trợ wildcard hoặc pattern matching.
7. Các setter thay thế toàn bộ danh sách tương ứng. ToolPolicy không xử lý việc đăng ký/gỡ Tool hoặc validation arguments.

Do đó, Registry mới tạo mặc định từ chối tất cả Tool cho đến khi được cấu hình Allow.

`get_definitions()` và `execute()` sử dụng cùng một ToolPolicy để tránh tình trạng Tool được cung cấp cho LLM nhưng lại không được phép thực thi. Composition Root chịu trách nhiệm cấu hình Allow list.

`get_definitions()` chỉ trả về những Tool được Policy cho phép; Tool bị từ chối không được đưa vào danh sách gửi cho LLM. Danh sách được sắp xếp ổn định theo tên.

### 3.6. Thứ tự xử lý execute()

Thứ tự xử lý bắt buộc:

1. Tìm Tool theo tên.
2. Nếu không tồn tại, trả về `RESOURCE_NOT_FOUND`; ToolPolicy không xử lý Tool không tồn tại.
3. Kiểm tra ToolPolicy.
4. Nếu không được phép, trả về `PERMISSION_DENIED`.
5. Validate arguments theo JSON Schema.
6. Nếu không hợp lệ, trả về `INVALID_ARGUMENTS`.
7. Thực thi Tool.
8. Trả về ToolResult.

Không được validate hoặc thực thi Tool trước khi kiểm tra quyền truy cập.

### 3.7. JSON Schema Validation

Registry thực hiện validation trước khi gọi Tool.

Hệ thống chỉ hỗ trợ một tập con JSON Schema, không tuyên bố tuân thủ toàn bộ tiêu chuẩn.

Các keyword hỗ trợ:

- `type`
- `properties`
- `required`
- `enum`
- `additionalProperties`
- `items`
- `minimum`
- `maximum`
- `minLength`
- `maxLength`
- `minItems`
- `maxItems`

Mọi schema node phải khai báo `type`. Schema root phải là JSON object. Các schema node và cấu trúc con được kiểm tra đệ quy. Keyword không được hỗ trợ hoặc keyword có kiểu dữ liệu không hợp lệ làm schema không hợp lệ và phải ném `ToolConfigError`.

Quy tắc kiểu dữ liệu:

- `integer` chỉ nhận JSON integer.
- `number` nhận integer hoặc floating-point.
- Không tự động ép kiểu.
- `5.0` không được chấp nhận như integer.

Quy tắc object:

- `properties` phải là JSON object; mỗi giá trị là schema hợp lệ. Nếu không khai báo `properties`, object vẫn được duyệt đệ quy để kiểm tra UTF-8 và độ sâu.
- `required` phải là array chứa các string duy nhất. Mọi tên trong `required` phải tồn tại trong `properties`, bất kể `additionalProperties`; danh sách required được kiểm tra theo thứ tự tên tăng dần byte-wise.
- `additionalProperties` chỉ nhận boolean, mặc định `true`. `false` từ chối mọi key không được khai báo trong `properties`; không hỗ trợ dạng schema object.

Quy tắc array:

- `items` là tùy chọn; nếu có, phải là schema hợp lệ và được áp dụng đệ quy cho từng phần tử. Không có `items` thì phần tử vẫn được duyệt để kiểm tra UTF-8 và độ sâu.
- `minItems` mặc định bằng `0`; `minItems` và `maxItems` phải là số nguyên không âm, và `minItems` không được lớn hơn `maxItems`.
- Mọi schema node có `type: array` bắt buộc khai báo `maxItems`, kể cả array lồng trong `items`, `properties` hoặc node schema khác.

Quy tắc scalar và enum:

- `minimum` và `maximum` nếu có phải là JSON number (không chấp nhận boolean), inclusive, áp dụng cho `integer` và `number`; `minimum` không được lớn hơn `maximum`. So sánh phải tránh mất chính xác và overflow.
- `minLength` và `maxLength` nếu có phải là số nguyên không âm; `minLength` không được lớn hơn `maxLength`. Độ dài string tính theo Unicode code points.
- `enum` phải là array không rỗng và mọi giá trị phải tương thích với `type`. So sánh dùng deep equality có xét kiểu: signed/unsigned integer được so sánh chính xác trong cùng nhóm integer; floating-point chỉ so sánh với floating-point; integer không bằng floating-point. Object và array so sánh đệ quy theo cùng quy tắc.

UTF-8 phải được kiểm tra đệ quy cho mọi JSON string value và mọi object key trong cả schema lẫn arguments, kể cả node không có `properties`/`items`. Tại mỗi node, kiểm tra depth trước, rồi UTF-8 của string value hoặc các key hiện tại; sau đó duyệt object theo key tăng dần byte-wise và array theo index tăng dần. UTF-8 không hợp lệ trong schema là `ToolConfigError`; trong arguments là `InvalidArguments`.

Giới hạn độ sâu tính trên cấu trúc JSON thực tế: schema tối đa 16, arguments tối đa 32; root JSON value có depth `0`. Mỗi lần đi qua object member hoặc array element đến value con thì depth tăng một. Object trung gian trong schema (bao gồm `properties`, tên property và `items`) cũng được tính; key không phải JSON value độc lập. Node vượt giới hạn trong schema gây `ToolConfigError`, còn arguments gây `InvalidArguments`. Node ở đúng giới hạn được chấp nhận.

Validator chỉ trả về lỗi đầu tiên theo thứ tự xác định sau:

1. Tại mỗi node: kiểm tra depth; UTF-8 tại node hiện tại; `type`; `enum`; rồi ràng buộc scalar theo thứ tự `minLength` rồi `maxLength`, `minimum` rồi `maximum`, hoặc `minItems` rồi `maxItems`.
2. Với object: kiểm tra `required` theo thứ tự tên tăng dần byte-wise; `additionalProperties` theo thứ tự key tăng dần byte-wise; rồi `properties` theo thứ tự key tăng dần byte-wise.
3. Với array: kiểm tra từng phần tử theo thứ tự index tăng dần; nếu có `items`, validate bằng schema của `items`, nếu không vẫn kiểm tra depth và UTF-8.

Lỗi validation dùng `field_path` theo JSON Pointer RFC 6901; root arguments có path `""`, mỗi object member thêm segment và mỗi array element thêm index. Key phải escape `~` thành `~0` và `/` thành `~1`. Với property required bị thiếu, path trỏ đến property đó; khi không xác định được field cụ thể có thể dùng root path `""`.

Schema configuration errors gồm: root không phải object; schema node thiếu hoặc có `type` không hỗ trợ; keyword không hỗ trợ; kiểu keyword sai; `properties` không phải object hoặc schema con không hợp lệ; `required` không phải array, có phần tử không phải string, tên trùng hoặc tham chiếu property chưa khai báo; `additionalProperties` không phải boolean; `items` không hợp lệ; array schema thiếu `maxItems`; giới hạn min/max không phải số nguyên không âm hoặc có min lớn hơn max; `minimum`/`maximum` không phải JSON number hoặc min lớn hơn max; enum không phải array, rỗng hoặc có giá trị không tương thích với type; UTF-8 không hợp lệ; schema vượt depth. Tất cả đều dùng `ToolConfigError`. Validation arguments lỗi trả `InvalidArguments`. Các điều kiện nghiệp vụ không biểu diễn được bằng subset này vẫn do Tool kiểm tra.

### 3.8. ToolResultSerializer

ToolResultSerializer chuyển ToolResult thành JSON compact để đưa vào conversation history.

Giới hạn mặc định: 8 KiB.

Không cắt trực tiếp chuỗi JSON đã serialize vì có thể tạo JSON không hợp lệ.

Khi dữ liệu vượt giới hạn, serializer tạo envelope JSON hợp lệ, chứa trạng thái truncated, kích thước ban đầu và preview được cắt tại ranh giới UTF-8.

Giới hạn 8 KiB không thay thế việc quản lý context/token budget của AgentLoop.

---

## 4. Environment

### 4.1. Kiến trúc

Namespace: `ogk::env`.

Các thành phần:

- `Environment`: abstract interface.
- `NativeEnvironment`: thao tác với hệ thống thực.
- `FakeEnvironment`: phục vụ unit test.
- `SandboxEnvironment`: thiết kế cho khả năng mở rộng trong tương lai.

### 4.2. NativeEnvironment

Linux là nền tảng được hỗ trợ đầy đủ trong phạm vi Phase 3.

Windows có thể hỗ trợ một số thao tác file; process execution không được cam kết tương đương Linux.

NativeEnvironment không phải sandbox hoàn chỉnh.

### 4.3. Process Execution

Tool `exec` không nhận một chuỗi shell tùy ý làm command.

Executable và arguments phải được truyền riêng.

Các giới hạn thiết kế:

- Executable phải được kiểm tra theo allowlist.
- Đường dẫn executable được chuẩn hóa trước khi so sánh.
- Không tin executable name hoặc PATH do input không đáng tin cậy cung cấp.
- Timeout mặc định 10 giây.
- Timeout tối đa 60 giây.
- stdout tối đa 1 MiB.
- stderr tối đa 1 MiB.
- Có cờ `truncated` khi output bị giới hạn.

Khi timeout hoặc vượt giới hạn output, process phải được kết thúc và thu hồi tài nguyên đúng cách.

Trên Linux cần xử lý process group, không chỉ process cha.

### 4.4. File Operations

- `read_file` chỉ đọc văn bản UTF-8.
- UTF-8 không hợp lệ dẫn đến lỗi `EXECUTION_FAILED`.
- Dữ liệu nhị phân chỉ được đọc qua API nội bộ phục vụ VisionAnalyzeTool.
- `write_file` mặc định không overwrite.
- Không tự tạo parent directory.
- Đường dẫn phải được chuẩn hóa và kiểm tra phạm vi truy cập.
- Cần xử lý `..` và symlink.

TOCTOU vẫn là giới hạn cần ghi nhận; không tuyên bố NativeEnvironment cung cấp bảo mật sandbox hoàn chỉnh.

---

## 5. Infrastructure

### 5.1. HttpClient

Thiết kế abstraction:

- `HttpClient`: interface.
- `CurlHttpClient`: implementation dùng libcurl.
- `MockHttpClient`: phục vụ test.

OllamaClient và WebSearchTool sử dụng HttpClient.

Giữ constructor hiện tại của OllamaClient và bổ sung overload nhận `std::unique_ptr<HttpClient>` để dependency injection.

Không thay đổi public interface của LLMClient.

### 5.2. MemoryStore

Thiết kế abstraction:

- `MemoryStore`: interface.
- `SQLiteMemoryStore`: implementation.

MemoryTool không truy cập SQLite trực tiếp.

SQLite sử dụng parameterized query.

`memory_search` sử dụng LIKE trong phạm vi MVP, không bắt buộc FTS5.

Dữ liệu:

- `memory_id`: int64.
- `created`: Unix timestamp UTC seconds.

### 5.3. Web Search

Backend: SearXNG.

Yêu cầu:

- Endpoint cấu hình từ bên ngoài.
- Không hardcode public instance.
- Hướng dẫn bật JSON output format.
- Có timeout.
- Giới hạn số kết quả.
- Giới hạn độ dài nội dung.

Kết quả tìm kiếm là dữ liệu không đáng tin cậy, không được coi là chỉ thị dành cho Agent.

---

## 6. Skill System

### 6.1. Thành phần

- `Skill`
- `SkillLoader`
- `SkillFrontMatterParser`
- `SkillSelector`
- `SkillInjector`
- `LoadReport`

Skill System không phụ thuộc trực tiếp AgentLoop.

### 6.2. SkillLoader

SkillLoader scan thư mục `skills/`, đọc Markdown và quản lý danh sách Skill.

Thứ tự scan phải ổn định.

Nếu xảy ra lỗi parse một Skill, Loader ghi nhận lỗi trong LoadReport và tiếp tục xử lý các Skill còn lại.

Nếu trùng tên Skill, giữ Skill xuất hiện đầu tiên và ghi nhận các bản trùng là lỗi.

### 6.3. Bộ Skill mặc định

Chốt thống nhất ba file:

1. `file_management.md`
2. `web_research.md`
3. `problem_solving.md`

Đây là bộ tên dùng trong cấu trúc thư mục, tài liệu thiết kế và test.

Không dùng lẫn với bộ tên cũ `task_planner.md`, `error_recovery.md`, `research_assistant.md`.

Tên Skill không phải yêu cầu bắt buộc được giảng viên chỉ định; đây là quyết định tổ chức của dự án.

### 6.4. Front Matter

Sử dụng parser cho một tập con YAML:

- `name`: chuỗi một dòng.
- `description`: chuỗi một dòng.
- `keywords`: danh sách `- item`.

Không hỗ trợ nested object, multiline scalar, anchor, alias hoặc cú pháp YAML nâng cao.

### 6.5. Skill Selection

Quy tắc matching:

- Lowercase ASCII.
- Delimiter gồm whitespace, punctuation, `_`, `-`.
- Giữ nguyên byte UTF-8 tiếng Việt.
- Không chuẩn hóa Unicode.
- Không loại bỏ dấu tiếng Việt.
- Từ có dấu và không dấu không được xem là tương đương.

Scoring:

- Phrase match: 2 điểm.
- Single-word match: 1 điểm.
- Mỗi keyword chỉ được tính tối đa một lần.
- Threshold tối thiểu: 1 điểm.
- Sắp xếp score giảm dần; nếu bằng điểm, sắp xếp theo tên Skill tăng dần.

**Quy tắc phrase overlap:**

Khi một phrase keyword được match, các single-word keyword nằm bên trong phrase đó của cùng Skill không được cộng điểm thêm.

Ví dụ:

Skill có keywords:

- `web search`
- `search`

Task chứa cụm `web search`.

Kết quả:

- `web search`: 2 điểm.
- `search`: 0 điểm bổ sung.
- Tổng: 2 điểm, không phải 3 điểm.

Quy tắc này chỉ áp dụng cho keyword đơn thuộc chính Skill đang được tính điểm.

### 6.6. Skill Injection

Giới hạn injection mặc định: 16 KiB.

- Xử lý Skill theo thứ tự score giảm dần và tên tăng dần.
- Chỉ inject nguyên vẹn một Skill nếu còn đủ ngân sách.
- Không cắt nội dung Markdown giữa chừng.
- Skill vượt ngân sách bị bỏ qua và ghi nhận trong danh sách budget-excluded.
- Skill đơn lẻ lớn hơn ngân sách cũng bị bỏ qua.

### 6.7. LoadReport

LoadReport chứa:

- Danh sách Skill load thành công.
- Danh sách lỗi `LoadIssue`.
- Danh sách warning `LoadIssue`.

Mỗi LoadIssue có path và reason.

---

## 7. Tool-specific Design

### 7.1. CalculatorTool

Hỗ trợ:

- `+`, `-`, `*`, `/`, `%`, `^`.
- Số nguyên và số thực.
- Unary `+`, `-`.
- Dấu ngoặc.
- `^` ưu tiên cao hơn unary `+`/`-` và kết hợp phải (P3-D51).

Quy tắc precedence phải cho kết quả: `-2^2 = -4`, `(-2)^2 = 4`, `2^3^2 = 512`, `2^-2 = 0.25`.

Ngữ pháp đề xuất để thực hiện các quy tắc trên:

```text
expression := additive
additive := multiplicative { ("+" | "-") multiplicative }
multiplicative := unary { ("*" | "/" | "%") unary }
unary := ("+" | "-") unary | power
power := primary [ "^" unary ]
primary := number | "(" expression ")"
```

`power` dùng đệ quy phải qua `unary`, do đó hỗ trợ số mũ âm và phép lũy thừa kết hợp phải. Không sử dụng `eval`.

Có giới hạn độ dài biểu thức và độ sâu parser; giá trị cụ thể được đề xuất trong Tool Specs và chưa Approved.

Xử lý chia cho 0, modulo không phải số nguyên và kết quả NaN/Infinity.

### 7.2. TodoListTool

Các operation:

- `add`
- `list`
- `complete`
- `remove`

MVP lưu trữ trong memory.

Schema khai báo đầy đủ các property được phép; validation điều kiện bắt buộc theo operation được thực hiện bên trong Tool.

Capacity tối đa là 100 task được lưu (P3-D52). Hành vi khi add vào danh sách đã đầy, gồm ErrorCode và xác nhận không sửa đổi danh sách, vẫn Proposed. ID generation và khả năng tái sử dụng ID cũng Proposed.

### 7.3. VisionAnalyzeTool

- Inject một LLMClient riêng.
- Có thể sử dụng model khác Agent model.
- Không sử dụng conversation history của Agent.
- Kiểm tra kích thước và định dạng ảnh.
- Kiểm tra khả năng vision của model trước khi sử dụng.

### 7.4. ClarifyTool

Sử dụng abstraction `UserInteraction`.

Không gọi trực tiếp `cin` hoặc `cout`.

Nếu không có interaction provider, trả về `INTERACTION_UNAVAILABLE`.

---

## 8. Cấu trúc thư mục đề xuất

```text
OGK-Agent/
├── CMakeLists.txt
├── include/
│   └── ogk/
│       ├── llm/
│       ├── tools/
│       ├── env/
│       ├── skills/
│       ├── infrastructure/
│       └── common/
├── src/
│   ├── llm/
│   ├── tools/
│   ├── env/
│   ├── skills/
│   └── infrastructure/
├── skills/
│   ├── file_management.md
│   ├── web_research.md
│   └── problem_solving.md
├── tests/
│   ├── unit/
│   └── integration/
├── docs/
│   ├── architecture.md
│   ├── phase-2-interface-spec.md
│   ├── phase-2-handover.md
│   ├── phase-3-overall-design.md
│   ├── phase-3-interface-spec.md
│   ├── phase-3-tool-specs.md
│   ├── phase-3-api-contracts.md
│   └── adr/
└── README.md
```

Cấu trúc trên là đề xuất tổ chức module. Khi triển khai cần giữ tương thích với cấu trúc thực tế của repository.

---

## 9. Dependency Graph

```mermaid
flowchart TD
    AgentLoop --> LLMClient
    AgentLoop --> ToolRegistry
    AgentLoop --> SkillInjector

    ToolRegistry --> Tool
    ToolRegistry --> ToolPolicy
    ToolRegistry --> ToolResultSerializer

    Tool --> Environment
    Tool --> MemoryStore
    Tool --> HttpClient
    VisionAnalyzeTool --> LLMClient
    ClarifyTool --> UserInteraction

    SkillInjector --> SkillSelector
    SkillSelector --> SkillLoader
    SkillLoader --> SkillFrontMatterParser

    NativeEnvironment -. implements .-> Environment
    FakeEnvironment -. implements .-> Environment
    SandboxEnvironment -. future .-> Environment

    CurlHttpClient -. implements .-> HttpClient
    MockHttpClient -. implements .-> HttpClient

    SQLiteMemoryStore -. implements .-> MemoryStore
```

Nguyên tắc dependency:

- Tool không phụ thuộc AgentLoop.
- Skill System không phụ thuộc AgentLoop.
- AgentLoop chỉ sử dụng abstraction.
- Environment được inject vào Tool.
- MemoryTool sử dụng MemoryStore abstraction.
- HTTP consumer sử dụng HttpClient abstraction.

---

## 10. Testing Strategy

### 10.1. Tool Tests

Mỗi Tool phải có:

- Test input hợp lệ.
- Test input không hợp lệ.
- Test một trường hợp thất bại đặc thù.

### 10.2. Registry Tests

Kiểm thử:

- Runtime registration.
- Duplicate registration.
- Lookup.
- Policy.
- Deny-over-Allow.
- Default-deny.
- JSON Schema validation.
- Exception handling.
- Tool không tồn tại.

Unregister không thuộc mandatory Registry tests hiện tại. Chỉ bổ sung test nếu chức năng này được human approval trước khi implementation.

### 10.3. Environment Tests

- Unit test dùng FakeEnvironment.
- Linux integration test dùng NativeEnvironment.
- Kiểm thử file, process, timeout và output limit.

### 10.4. Infrastructure Tests

- Mock HTTP cho Ollama và SearXNG.
- SQLite integration test với temporary database.
- Serializer JSON hợp lệ và xử lý truncation.

SearXNG integration test thực tế nếu có môi trường phù hợp. Nếu không chạy, phải ghi `NOT RUN`, không ghi `PASS`.

### 10.5. Skill Tests

Kiểm thử:

- Front matter parsing.
- Invalid Markdown metadata.
- Duplicate Skill.
- Keyword matching.
- Phrase matching.
- Phrase overlap.
- Score tie-break.
- Injection budget.
- LoadReport.

### 10.6. Regression

Toàn bộ Phase 2 tests phải tiếp tục PASS.

---

## 11. Design Patterns

| Pattern | Vị trí | Mục đích |
|---|---|---|
| Strategy | Tool/Policy và các abstraction phù hợp | Thay thế hành vi qua interface |
| Registry/Factory | ToolRegistry | Đăng ký và quản lý Tool |
| Dependency Injection | Environment, HttpClient, MemoryStore | Giảm coupling và hỗ trợ test |
| Observer/Hook | AgentLoop ở Phase 4 | Cho Harness quan sát step |
| Template Method | AgentLoop ở Phase 4 | Định nghĩa skeleton ReAct |
| Strategy | Evaluator ở Phase 5 | Thay thế cách đánh giá |

Phase 3 tập trung Tool và Skill. Không triển khai AgentLoop hoặc Harness trong phạm vi này.

---

## 12. Decision Register

### 12.1. Quy ước

Chỉ sử dụng ba trạng thái:

- `Approved`: đã được phê duyệt.
- `Proposed`: đang đề xuất, chưa được phê duyệt.
- `Deferred`: chủ động hoãn sang giai đoạn sau.

Nguồn quyết định:

- `Đề bài`: yêu cầu trực tiếp từ giảng viên.
- `Người dùng`: lựa chọn được người thực hiện xác nhận.
- `Thiết kế`: đề xuất kỹ thuật của nhóm thiết kế.

Không dùng `Required` làm trạng thái.

`TBD` được dùng trong mô tả hoặc ghi chú để chỉ nội dung chưa xác định; đây không phải trạng thái quyết định thứ tư.

### 12.2. Các quyết định đã xác nhận trong phạm vi tài liệu hiện tại

| ID | Quyết định | Trạng thái | Nguồn |
|---|---|---|---|
| P3-D01 | Tool có interface thống nhất | Approved | Đề bài |
| P3-D02 | ToolRegistry hỗ trợ runtime registration | Approved | Đề bài |
| P3-D03 | Registry sử dụng `std::unique_ptr` ownership | Approved | Người dùng |
| P3-D04 | Bổ sung `todo_list` | Approved | Người dùng |
| P3-D05 | Bổ sung `vision_analyze` | Approved | Người dùng |
| P3-D06 | Bổ sung `clarify` | Approved | Người dùng |
| P3-D07 | Web search sử dụng SearXNG | Approved | Người dùng |
| P3-D08 | Deny-over-Allow và default-deny | Approved | Người dùng |
| P3-D09 | JSON Schema validation trước khi execute | Approved | Thiết kế |
| P3-D10 | ToolResult có cấu trúc success/error | Approved | Thiết kế |
| P3-D11 | Registry sở hữu Tool bằng unique_ptr | Approved | Người dùng |
| P3-D12 | NativeEnvironment triển khai tối thiểu trên Linux | Approved | Người dùng |
| P3-D13 | FakeEnvironment dùng cho unit test | Approved | Thiết kế |
| P3-D14 | SandboxEnvironment chỉ thiết kế, chưa triển khai đầy đủ | Deferred | Người dùng |
| P3-D15 | HttpClient abstraction và CurlHttpClient | Approved | Thiết kế |
| P3-D16 | MockHttpClient phục vụ kiểm thử | Approved | Thiết kế |
| P3-D17 | MemoryStore abstraction | Approved | Thiết kế |
| P3-D18 | SQLiteMemoryStore | Approved | Thiết kế |
| P3-D19 | memory_search sử dụng SQLite LIKE trong MVP | Approved | Thiết kế |
| P3-D20 | ToolResultSerializer giới hạn mặc định 8 KiB | Approved | Thiết kế |
| P3-D21 | SkillLoader đọc Markdown từ skills/ | Approved | Đề bài |
| P3-D22 | Skill selection sử dụng keyword matching | Approved | Đề bài |
| P3-D23 | Skill injection không phụ thuộc trực tiếp AgentLoop | Approved | Đề bài |
| P3-D24 | Chốt bộ tên Skill thống nhất trong repository | Approved | Người dùng |
| P3-D25 | Phrase overlap không cộng thêm điểm keyword đơn bên trong phrase | Approved | Người dùng |
| P3-D26 | ToolPolicy sử dụng Allow list và Deny list | Approved | Interface Specification |
| P3-D27 | Allow list mặc định rỗng, mặc định deny all | Approved | Interface Specification |
| P3-D28 | Deny list có quyền ưu tiên cao hơn Allow list | Approved | Interface Specification |
| P3-D29 | Tool name được so sánh chính xác, phân biệt hoa thường; không wildcard | Approved | Interface Specification |
| P3-D30 | ToolPolicy không xử lý Tool không tồn tại | Approved | Interface Specification |
| P3-D31 | Setter thay thế toàn bộ danh sách tương ứng | Approved | Interface Specification |
| P3-D32 | Policy dùng chung cho execute và get_definitions; Composition Root cấu hình Allow list | Approved | Interface Specification |
| P3-D33 | Chỉ hỗ trợ tập con JSON Schema được liệt kê tại Interface Specification Section 2.3 | Approved | Interface Specification |
| P3-D34 | Mọi schema node bắt buộc có type; hỗ trợ object, array, string, integer, number, boolean, null | Approved | Interface Specification |
| P3-D35 | Object không có properties được phép nhận key tùy theo additionalProperties; vẫn kiểm tra UTF-8 và depth | Approved | Interface Specification |
| P3-D36 | Mọi array schema bắt buộc khai báo maxItems, áp dụng đệ quy | Approved | Interface Specification |
| P3-D37 | minItems mặc định 0; minItems và maxItems phải hợp lệ và minItems không vượt maxItems | Approved | Interface Specification |
| P3-D38 | minimum và maximum inclusive, áp dụng cho integer và number; so sánh phải tránh mất chính xác và overflow | Approved | Interface Specification |
| P3-D39 | String length tính theo Unicode code points | Approved | Interface Specification |
| P3-D40 | Arguments và schema phải được kiểm tra UTF-8 ở mọi string value và object key | Approved | Interface Specification |
| P3-D41 | Enum sử dụng deep equality có xét kiểu dữ liệu; integer và float không tự động bằng nhau | Approved | Interface Specification |
| P3-D42 | Validator chỉ trả lỗi đầu tiên theo thứ tự validation cố định | Approved | Interface Specification |
| P3-D43 | field_path sử dụng JSON Pointer RFC 6901 | Approved | Interface Specification |
| P3-D44 | Schema configuration errors dùng ToolConfigError thống nhất | Approved | Interface Specification |
| P3-D45 | Schema depth tối đa 16; arguments depth tối đa 32; root depth 0, tính theo cấu trúc JSON | Approved | Interface Specification |
| P3-D46 | validate() chỉ nhận schema đã được validate; Registry chịu trách nhiệm cache schema và trạng thái validate | Approved | Interface Specification |
| P3-D47 | Unsupported keywords và các tính năng ngoài subset không được triển khai | Approved | Interface Specification |
| P3-D48 | Required phải là danh sách string duy nhất và mọi tên đều phải khai báo trong properties | Approved | Interface Specification |
| P3-D49 | additionalProperties chỉ nhận boolean; mặc định true; false từ chối key không khai báo | Approved | Interface Specification |
| P3-D50 | stdout và stderr của process phải là UTF-8 hợp lệ; nếu một trong hai stream không hợp lệ, trả `ExecutionFailed` và không đưa nội dung của stream nào vào kết quả | Approved | Người dùng |
| P3-D51 | `^` ưu tiên cao hơn unary `+`/`-`, kết hợp phải; unary vẫn hợp lệ ở phía số mũ | Approved | Người dùng |
| P3-D52 | TodoList MVP lưu tối đa 100 task | Approved | Người dùng |

**Lưu ý về Decision Register:** P3-D01 đến P3-D25 là các quyết định đã được xác nhận trong ngữ cảnh tài liệu hiện tại; P3-D26 đến P3-D49 đồng bộ theo Interface Specification đã Approved. P3-D50 đến P3-D52 là các quyết định được người dùng phê duyệt trong đợt hoàn thiện thiết kế này. Historical reconciliation: **Unavailable — No historical source provided.** Không có nguồn lịch sử độc lập để xác minh mapping giữa register P3-D01–D40 cũ và register hiện tại; historical mapping và historical approval status là **Unknown / Unverified**. Không suy luận mapping dựa trên ID trùng nhau hoặc trạng thái hiện tại. Kết luận này không thay đổi nội dung hoặc trạng thái của các quyết định hiện tại.

### 12.3. Các nội dung còn Proposed hoặc Deferred

Kết quả đối chiếu register lịch sử là **Unavailable — No historical source provided**; mapping và historical approval status là **Unknown / Unverified**. Đây là kết quả kiểm toán tài liệu, không phải một quyết định thiết kế đang chờ xử lý. Không còn hành động tìm kiếm hoặc tái tạo historical register trong danh sách pending.

| Nội dung | Trạng thái | Nguồn |
|---|---|---|
| Error mapping khi TodoList đạt capacity 100 và hành vi không thay đổi trạng thái khi add thất bại | Proposed | Thiết kế |
| Chi tiết API, JSON Schema, output, error mapping và test contract cho từng Tool | Proposed | Thiết kế |
| Các API contract cụ thể cho ToolRegistry, Environment, HttpClient, MemoryStore và UserInteraction | Proposed | Thiết kế |
| ID generation/reuse policy của TodoList | Proposed | Thiết kế |
| Các chi tiết chưa được quyết định trong phase-3-tool-specs.md và phase-3-api-contracts.md (nội dung cụ thể còn chưa xác định được ghi TBD) | Proposed | Thiết kế |

Không được coi các nội dung Proposed/TBD trong bảng hoặc tài liệu đặc tả là phê duyệt mặc định.

---

## 13. Implementation Plan

Phase 3 chưa được triển khai. Chỉ bắt đầu implementation sau khi các tài liệu thiết kế bắt buộc được phê duyệt; hiện Overall Design vẫn là Design Draft và các nội dung còn Proposed/Deferred chưa được xem là quyết định được duyệt.

Triển khai theo thứ tự sau khi điều kiện phê duyệt được đáp ứng:

1. Khóa tài liệu thiết kế và Interface Specification.
2. Tạo Tool interface và ToolResult.
3. Xây dựng ToolPolicy.
4. Xây dựng JSON Schema validator.
5. Xây dựng ToolRegistry.
6. Xây dựng Environment abstraction và FakeEnvironment.
7. Xây dựng NativeEnvironment.
8. Xây dựng HttpClient và MemoryStore.
9. Triển khai các Tool.
10. Xây dựng Skill System.
11. Hoàn thiện unit tests và integration tests.
12. Kiểm tra regression với Phase 2 và viết báo cáo bàn giao.

Không bắt đầu implementation trước khi tài liệu thiết kế và interface được phê duyệt.

---

## 14. Known Limitations

1. NativeEnvironment không phải sandbox hoàn chỉnh.
2. Windows process execution chưa được cam kết.
3. JSON Schema chỉ hỗ trợ subset đã mô tả.
4. Skill matching không chuẩn hóa dấu tiếng Việt.
5. SQLite LIKE chưa phải semantic search.
6. ToolResultSerializer không thay thế context/token budget management.
7. SearXNG phụ thuộc cấu hình endpoint và JSON output.
8. VisionAnalyzeTool phụ thuộc model có khả năng xử lý ảnh.
9. TOCTOU trong file access vẫn là rủi ro cần ghi nhận.

---

## 15. Kết luận và trạng thái phê duyệt

Tài liệu này thống nhất kiến trúc tổng thể Phase 3, trách nhiệm các module, hướng dependency, nguyên tắc kiểm thử và các quyết định đã xác nhận.

Historical reconciliation đã kết thúc với trạng thái **Unavailable — No historical source provided**; mapping và historical approval status vẫn **Unknown / Unverified**. Không có hành động tìm hoặc tái tạo historical register nào đang chờ xử lý.

Trước khi chuyển sang implementation, cần:

1. Hoàn thiện và phê duyệt các hợp đồng trong `phase-3-tool-specs.md` và `phase-3-api-contracts.md`.
2. Xử lý các nội dung Proposed/TBD và phê duyệt phiên bản cuối của Overall Design.
3. Chỉ sau khi các tài liệu thiết kế bắt buộc được phê duyệt, bắt đầu triển khai Phase 3.

**Trạng thái hiện tại: Design Draft — Chưa khóa thiết kế, chưa bắt đầu triển khai Phase 3.**