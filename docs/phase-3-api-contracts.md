# Phase 3 API Contracts

**Version:** Draft  
**Status:** Design Draft  
**Language:** C++23  
**Scope:** Proposed minimum API contracts for ToolRegistry, Environment, HttpClient, MemoryStore, and UserInteraction.

## 1. Status and constraints

This document records API proposals needed to complete Phase 3 design review. It does not change Architecture v1.0, the Approved Interface Specification, CT-01–CT-11, P3-D01–P3-D49, or Phase 2 public interfaces.

The following are existing approved constraints, not new decisions in this document:

- ToolRegistry owns registered Tools using `std::unique_ptr`, validates and caches each Tool schema at registration, uses one ToolPolicy for both definitions and execution, filters definitions to permitted Tools, and sorts definitions by name.
- Execution order is lookup, policy, validation, then execution. The Registry is synchronous and not thread-safe.
- NativeEnvironment is not a complete sandbox. Linux is the fully supported platform for Phase 3; Windows process execution is not guaranteed equivalent.
- HttpClient has CurlHttpClient and MockHttpClient implementations. OllamaClient keeps its existing constructor and may add an overload that accepts `std::unique_ptr<HttpClient>`. The public LLMClient interface does not change.
- MemoryTool uses MemoryStore; SQLiteMemoryStore uses parameterized SQL. MVP memory search uses SQLite LIKE, not FTS5.
- ClarifyTool uses UserInteraction, does not call `cin` or `cout`, and returns `InteractionUnavailable` when no provider is available.
- P3-D50, P3-D51, and P3-D52 are Approved in the Overall Design: UTF-8 process output behavior, calculator precedence, and TodoList capacity 100. Detailed contracts beyond those approved statements remain Proposed/TBD.

Unless explicitly identified above, API names, signatures, field shapes, exception semantics, limits, and behaviors below are **Proposed** or **TBD**, not Approved.

## 2. ToolRegistry

### 2.1. Proposed public surface

```cpp
namespace ogk::tools
{
    struct ToolDefinition
    {
        std::string name;
        std::string description;
        nlohmann::json parameters;
    };

    class ToolRegistry
    {
    public:
        explicit ToolRegistry(ToolPolicy policy = {});

        void register_tool(std::unique_ptr<Tool> tool);

        std::vector<ToolDefinition> get_definitions() const;

        ToolResult execute(
            std::string_view tool_name,
            const nlohmann::json& arguments
        );
    };
}
```

The proposed Registry owns its ToolPolicy value. Composition Root configures that policy before constructing the Registry. Both `get_definitions()` and `execute()` consult that same stored policy. This is a proposed ownership choice; exact construction/configuration API remains subject to review.

No public raw-pointer lookup or unregister API is proposed. Name lookup is internal to execution so callers do not retain references whose lifetime can be invalidated. The Overall Design draft lists unregister as a Registry responsibility, but no approved API signature or lifecycle requirement is established; whether to expose unregister remains unresolved and no function is added here.

### 2.2. Registration and ownership

- On successful registration, the Registry owns the Tool.
- The Registry obtains `name()` and `parameters()`, validates the schema once, and caches the validated schema and validation state.
- A Tool schema that fails validation raises `ToolConfigError`; it is not converted to a per-call `ToolResult`.
- A null Tool pointer and duplicate Tool name are Proposed configuration errors. Proposed behavior is to throw `ToolConfigError`; whether duplicate registration may replace an existing Tool is not approved, and this proposal rejects replacement.
- Exact behavior for a Tool whose `name()` or `parameters()` throws during registration is TBD; it must not leave a partially registered entry.
- Tool names are exact and case-sensitive under the Approved ToolPolicy contract.

### 2.3. Definitions

`get_definitions()` returns a value containing `{name, description, parameters}` for each permitted Tool only. Results are sorted in ascending Tool-name order. A denied Tool is not exposed to the LLM.

The returned definitions are snapshots; callers do not receive mutable references to Registry-owned Tools or cached schemas.

### 2.4. Execution

`execute(name, arguments)` follows this exact order:

1. Lookup by exact Tool name. Missing Tool returns `ResourceNotFound`.
2. Check the Registry's ToolPolicy. Denial returns `PermissionDenied`.
3. Validate arguments using the schema validated and cached at registration. Failure returns `InvalidArguments` with the validator's first error and JSON Pointer path.
4. Invoke `Tool::execute()`.
5. Return the ToolResult. An unexpected exception is logged internally and converted to `InternalError`; raw exception details are not exposed to the LLM.

Expected business errors are returned by the Tool as ToolResult. `ToolConfigError` remains a configuration exception, not an ErrorCode.

### 2.5. Execution model and tests

The Registry is synchronous and not thread-safe. Concurrent registration, policy mutation, or execution against one Registry is unsupported.

Proposed contract tests:

| Case | Expected result |
|---|---|
| Register a valid Tool | Schema validated once; Tool becomes Registry-owned |
| Register invalid schema | `ToolConfigError`; Tool is not exposed in definitions |
| Register duplicate name | Proposed `ToolConfigError`; existing registration is unchanged |
| Get definitions with default-deny policy | Empty list |
| Allow one of two Tools | Only the permitted Tool is returned, in stable name order |
| Execute missing name | `ResourceNotFound`; Tool is not called |
| Execute denied Tool | `PermissionDenied`; validation and Tool execution do not occur |
| Execute invalid arguments | `InvalidArguments`; Tool is not called |
| Tool returns expected error | Same ToolResult is returned |
| Tool throws unexpectedly | `InternalError`; exception detail is not returned to the LLM |

Duplicate-registration exception type and replacement semantics remain Proposed.

## 3. Environment and NativeEnvironment

### 3.1. Proposed minimal interface

```cpp
namespace ogk::env
{
    struct ProcessRequest
    {
        std::string executable;
        std::vector<std::string> arguments;
        std::chrono::milliseconds timeout;
        std::size_t stdout_limit_bytes;
        std::size_t stderr_limit_bytes;
    };

    enum class ProcessStopReason
    {
        Exited,
        TimedOut,
        OutputLimit
    };

    struct ProcessResult
    {
        ProcessStopReason stop_reason;
        std::optional<int> exit_code;
        std::vector<std::byte> stdout_bytes;
        std::vector<std::byte> stderr_bytes;
        bool truncated;
    };

    class Environment
    {
    public:
        virtual ~Environment() = default;

        virtual std::string read_text(std::string_view path) = 0;
        virtual void write_text(
            std::string_view path,
            std::string_view content,
            bool overwrite
        ) = 0;
        virtual std::vector<std::byte> read_binary(
            std::string_view path
        ) = 0;
        virtual ProcessResult run_process(
            const ProcessRequest& request
        ) = 0;
    };
}
```

All signatures and result types above are Proposed. `read_binary()` is an internal capability for VisionAnalyzeTool; binary data is not exposed as a Tool argument or returned directly to the LLM.

### 3.2. Process behavior

Existing design limits are a 10-second default timeout, a 60-second maximum timeout, and 1 MiB maximum for each output stream. Executable and argv remain separate; arbitrary shell command strings are prohibited. The executable is checked against an allowlist after path normalization. On Linux, timeout/output-limit termination must clean up the process group, not only the parent.

Proposed API choice: timeout and byte limits are supplied by trusted Tool/Environment configuration, not by `exec` arguments. `exec` has no timeout field in its proposed JSON Schema. The configured timeout must be greater than zero and at most 60 seconds; the default is 10 seconds.

`ProcessResult` carries raw bytes so ExecTool can apply P3-D50. The proposed behavior for output validation and termination is detailed in `phase-3-tool-specs.md`. Exit code is absent when the process is forcibly terminated before a normal exit.

### 3.3. File behavior and unresolved configuration

Text reads return UTF-8 text; invalid UTF-8 maps to `ExecutionFailed`. Writes do not overwrite by default and do not create missing parent directories. The proposed `overwrite` argument is applied by the Tool, not by JSON Schema defaults.

The following remain Proposed/TBD and must be fixed before implementation:

- How allowed roots are configured and whether relative paths are resolved against a configured working directory.
- Exact path normalization and canonicalization rules.
- Symlink behavior, including links that resolve outside an allowed root.
- The residual TOCTOU limitation; NativeEnvironment is not a complete security sandbox.
- Exact file errors and their Tool ErrorCode mappings.
- Atomicity expectations for writes and behavior if overwrite races with another writer.
- Process launch error classification.
- Platform-specific process guarantees beyond the existing Linux support statement.

Proposed tests include text read/write using FakeEnvironment; invalid UTF-8 read; missing/out-of-scope paths; overwrite on/off; absent parent directory; binary image read; process exit, timeout, output cap, invalid UTF-8 output, and process-group cleanup on Linux.

## 4. HttpClient

### 4.1. Proposed request/response contract

```cpp
namespace ogk::infrastructure
{
    struct HttpRequest
    {
        std::string method;
        std::string url;
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;
        std::chrono::milliseconds timeout;
        std::size_t response_limit_bytes;
    };

    struct HttpResponse
    {
        long status_code;
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;
    };

    class HttpClient
    {
    public:
        virtual ~HttpClient() = default;
        virtual HttpResponse send(const HttpRequest& request) = 0;
    };
}
```

The types and signature are Proposed. CurlHttpClient implements this interface; MockHttpClient provides deterministic responses and failures for tests.

Transport failures and HTTP status responses are distinct: a received non-success HTTP status remains an HttpResponse; connection, timeout, and response-limit failures are transport errors. The exact error type/exception contract and which HTTP status codes each consumer treats as failure are TBD.

### 4.2. Compatibility and unresolved behavior

- Preserve the existing OllamaClient constructor.
- Add the already planned dependency-injection overload accepting `std::unique_ptr<HttpClient>`.
- Do not change the public LLMClient interface.
- WebSearchTool obtains its SearXNG endpoint from external configuration; do not hardcode a public instance.
- Configure SearXNG JSON output. Search results are untrusted data, not Agent instructions.

Redirect behavior, TLS/configuration policy, timeout values, response-size limits, header normalization, and HTTP error mapping are Proposed/TBD. Any numeric Web Search timeout/result/content limits proposed in Tool Specs are not Approved.

Proposed tests: mock success, non-success HTTP status, transport failure, timeout, response-size limit, and serialization/parsing failure. Live SearXNG integration remains conditional on a suitable environment and must be reported `NOT RUN` when omitted.

## 5. MemoryStore

### 5.1. Proposed record and operations

```cpp
namespace ogk::infrastructure
{
    struct MemoryRecord
    {
        std::int64_t memory_id;
        std::string content;
        std::vector<std::string> tags;
        std::int64_t created;
    };

    class MemoryStore
    {
    public:
        virtual ~MemoryStore() = default;

        virtual MemoryRecord save(
            std::string_view content,
            const std::vector<std::string>& tags
        ) = 0;

        virtual std::vector<MemoryRecord> search(
            std::string_view query,
            std::size_t limit
        ) = 0;
    };
}
```

These method names and signatures are Proposed. `memory_id` remains int64 and `created` is Unix timestamp UTC seconds, consistent with the Overall Design. MemoryTool depends on MemoryStore and does not issue SQLite queries directly. SQLiteMemoryStore uses parameterized SQL.

### 5.2. Search and persistence decisions

MVP search uses SQLite LIKE; FTS5 is not required. The exact matching semantics are Proposed:

- Candidate behavior: literal substring matching by wrapping the escaped query in `%...%`.
- Candidate escaping: escape `%`, `_`, and the selected escape character so they are treated literally.
- Candidate case behavior: SQLite's default LIKE behavior (ASCII case-insensitive; non-ASCII case behavior follows SQLite build/configuration).
- Candidate ordering: `created` descending, then `memory_id` descending for deterministic ties.
- Candidate duplicate behavior: allow duplicate content; no deduplication or uniqueness guarantee.

Those candidates are not Approved. Empty query behavior, tags filtering, transaction boundaries, schema migration, storage errors, and result-limit enforcement remain TBD. Proposed save/search tests cover persistence, tags, literal wildcard characters, case behavior, ordering ties, limit, empty query, duplicate content, and database failure.

## 6. UserInteraction

### 6.1. Proposed abstraction

```cpp
namespace ogk::tools
{
    struct InteractionRequest
    {
        std::string question;
        std::vector<std::string> options;
    };

    enum class InteractionKind
    {
        FreeText,
        SelectedOption,
        Cancelled
    };

    struct InteractionResult
    {
        InteractionKind kind;
        std::string answer;
        std::optional<std::size_t> selected_option_index;
    };

    class UserInteraction
    {
    public:
        virtual ~UserInteraction() = default;
        virtual InteractionResult ask(
            const InteractionRequest& request
        ) = 0;
    };
}
```

Names and shapes are Proposed. ClarifyTool uses this abstraction and must not call `cin` or `cout`. If no provider exists, ClarifyTool returns the existing `InteractionUnavailable` ErrorCode.

The following remain Proposed/TBD and must not be inferred from empty strings or null values: cancellation representation; whether free-text answers may be empty; whether selected option is returned by index, exact text, or both; provider timeout behavior; and whether a provider error is an interaction failure or an unexpected exception.

Proposed tests cover free-text response, selected choice, zero options, cancellation, empty response, missing provider, provider timeout, and provider exception. Expected ToolResult data for cancellation and empty answers remains unapproved pending the Tool Specs decisions.

## 7. Dependency direction and readiness

Proposed dependency direction, consistent with the Overall Design:

- Tools depend on Environment, HttpClient, MemoryStore, LLMClient (vision only), or UserInteraction through injected abstractions.
- NativeEnvironment, CurlHttpClient, SQLiteMemoryStore, and concrete interaction providers implement or fulfill those abstractions.
- ToolRegistry owns Tools and applies ToolPolicy and schema validation; Tools do not call the Registry or AgentLoop.
- No Phase 2 public interface is changed.

This document is **Design Draft**. Its API signatures and unresolved behavior require review and approval before implementation. This draft does not mark its Proposed contracts Approved.
