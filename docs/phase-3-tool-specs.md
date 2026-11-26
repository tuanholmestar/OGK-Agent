# Phase 3 Tool Specifications

**Version:** Draft  
**Status:** Design Draft  
**Language:** C++23  
**Scope:** Proposed operation-level contracts for the ten Phase 3 Tools.

## 1. Status, common contracts, and schema rules

This document completes a design draft for review. A complete-looking contract is not an Approved contract. Unless a rule is explicitly identified as Approved or as a constraint already stated in the Overall Design, newly specified fields, bounds, outputs, error mappings, and behaviors are **Proposed** or **TBD**.

### 1.1. Approved shared contracts

- The Interface Specification v1.0 remains Approved and unchanged. Tool implements `name()`, `description()`, `parameters()`, and `execute(const nlohmann::json&)`.
- Each parameter schema root is a JSON object; every schema node has `type`; only the keyword subset in Interface Specification §2.3 is supported.
- `additionalProperties` is boolean and defaults to `true` under the validator contract. The schemas proposed here explicitly set it to `false` to reject undeclared arguments; this per-Tool choice remains Proposed.
- `required` entries must name properties declared in `properties`. Conditional requirements are checked by the Tool because the approved schema subset has no conditional schema keywords.
- Every array schema node has its own `maxItems`, including nested arrays. `exec.arguments.maxItems=64` and `clarify.options.maxItems=10` are the approved limits recorded by Interface Specification §2.15.3. The value `tags.maxItems=20` is approved there, but assignment of `tags` to a Tool is Proposed below.
- Schema validation, UTF-8 checks, depth limits, deterministic first-error ordering, and JSON Pointer paths follow Interface Specification §2.
- ToolResult success data is always a JSON object. Error results have JSON null data and ErrorInfo. Unexpected exceptions become `InternalError`; raw exception details are not exposed to the LLM.
- The runtime ErrorCodes are `InvalidArguments`, `PermissionDenied`, `ResourceNotFound`, `ResourceUnavailable`, `ExecutionFailed`, `Timeout`, `InternalError`, and `InteractionUnavailable`.

No schema below uses `default`, `pattern`, `format`, `oneOf`, `uniqueItems`, or any other unsupported keyword. Any stated default is applied by Tool implementation, not by JSON Schema.

### 1.2. Cross-cutting Proposed conventions

- Each schema below explicitly sets `additionalProperties: false`; review may choose otherwise.
- Proposed Tool-level validation failures return `InvalidArguments` with a field path where a particular argument is responsible.
- Proposed external service/storage failures map to `ResourceUnavailable`; exact mappings remain Proposed unless identified as an existing contract.
- Error messages in examples are descriptive, not stable API strings. Stable ErrorCode strings remain governed by the Approved Interface Specification.

## 2. `exec`

### 2.1. Purpose

Runs one executable with a separate argument vector. Arbitrary shell command strings are not accepted. The executable is checked against an allowlist.

### 2.2. Arguments and proposed JSON Schema

Fields:

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `executable` | string | Yes | Executable path/name to run; allowlist and normalization apply |
| `arguments` | array of string | Yes | Arguments passed separately to the executable |

Timeout and output limits are supplied by trusted Tool/Environment configuration, not Tool arguments (**Proposed**). No `timeout` argument is exposed.

```json
{
  "type": "object",
  "properties": {
    "executable": {
      "type": "string"
    },
    "arguments": {
      "type": "array",
      "items": {
        "type": "string"
      },
      "minItems": 0,
      "maxItems": 64
    }
  },
  "required": ["arguments", "executable"],
  "additionalProperties": false
}
```

The argument-array bound 64 is Approved. The field type, requiredness, `minItems`, and closed-object choice are Proposed elaborations.

### 2.3. Success `ToolResult.data`

Proposed success object:

```json
{
  "exit_code": 0,
  "stdout": "text",
  "stderr": "",
  "truncated": false
}
```

`exit_code` is an integer for a normally exited process and `null` if it was forcibly terminated due to an output limit (**Proposed**). A non-zero exit code remains a successful process invocation and is returned in this object (**Proposed**). Timeout is an Error result, not a success object.

### 2.4. Error mapping and process rules

| Condition | Result | Status |
|---|---|---|
| Registry denies Tool | `PermissionDenied` | Approved Registry contract |
| Executable rejected by executable allowlist | `PermissionDenied` | Proposed mapping |
| Process exceeds configured timeout | `Timeout` | Proposed Tool mapping; timeout requirement exists in Overall Design |
| Process cannot be launched | `ExecutionFailed` | Proposed |
| Either captured stream contains invalid UTF-8 | `ExecutionFailed` | Approved by P3-D50 |
| Process exits non-zero | Success with non-zero `exit_code` | Proposed |
| Output limit reached | Terminate process, return captured output with `truncated=true` | Proposed |

P3-D50 requires valid UTF-8 for both stdout and stderr. If either stream contains invalid UTF-8, the Tool returns `ExecutionFailed` and exposes neither stream, even if the other stream was valid. The error message is generic; invalid bytes and raw process details are not returned.

Preserve the 10-second default timeout, 60-second maximum timeout, 1 MiB stdout limit, and 1 MiB stderr limit. On timeout or output-limit termination, terminate and reclaim resources; on Linux, handle the process group, not only the parent. For output-limit truncation, the proposed behavior is to discard any trailing partial UTF-8 code point created solely by the byte boundary, set `truncated=true`, and not classify that boundary fragment as malformed UTF-8. Invalid UTF-8 observed before the truncation boundary still produces `ExecutionFailed`.

Timeout/output-limit behavior when both conditions occur together, and exact process launch error classification, remain Proposed/TBD.

### 2.5. Validation/business rules

- `executable` and each argument are UTF-8 validated by the approved Registry validator.
- Do not invoke a shell with a command-string argument.
- Normalize executable paths before allowlist comparison; do not trust a PATH or executable name supplied by untrusted input.
- Tool/Environment configuration supplies timeout and byte limits; timeout is greater than zero and at most 60 seconds.
- stdout and stderr are capped independently at 1 MiB.

Executable path normalization rules, allowlist configuration source, and process-launch error distinctions remain Proposed/TBD.

### 2.6. Proposed tests

| Test | Expected outcome |
|---|---|
| Allowed executable, zero arguments, exit 0 | Success; exit code 0; valid stdout/stderr |
| Allowed executable exits non-zero | Proposed success object with that exit code |
| Executable not on allowlist | `PermissionDenied`; process not launched |
| Process exceeds 10-second configured default | `Timeout`; process/process group terminated |
| stdout exceeds 1 MiB limit | Process terminated; captured output returned with `truncated=true` |
| stderr exceeds 1 MiB limit | Same limit behavior independently of stdout |
| stdout contains invalid UTF-8; stderr is valid | `ExecutionFailed`; neither stream included |
| stderr contains invalid UTF-8; stdout is valid | `ExecutionFailed`; neither stream included |
| Both streams contain invalid UTF-8 | `ExecutionFailed`; neither stream included |
| Invalid byte occurs before output cap | `ExecutionFailed`, even if the other stream is valid |
| UTF-8 code point crosses a truncation boundary | Proposed drop of incomplete trailing code point; `truncated=true` |
| Executable cannot be launched | Proposed `ExecutionFailed` |

## 3. `read_file`

### 3.1. Purpose

Reads a text file as UTF-8 using the Read operation of FileTool. It does not expose binary file content.

### 3.2. Arguments and proposed JSON Schema

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `path` | string | Yes | Path of the text file |

```json
{
  "type": "object",
  "properties": {
    "path": {
      "type": "string"
    }
  },
  "required": ["path"],
  "additionalProperties": false
}
```

Exact field naming, closed-object behavior, and allowed-root resolution remain Proposed.

### 3.3. Success `ToolResult.data`

Proposed success object:

```json
{
  "content": "UTF-8 file contents"
}
```

`content` is the complete text returned by the Environment read operation. A file-size limit is TBD.

### 3.4. Error mapping

| Condition | ErrorCode | Status |
|---|---|---|
| Invalid UTF-8 in file | `ExecutionFailed` | Required by current Overall Design and task |
| File does not exist | `ResourceNotFound` | Proposed |
| Path is outside allowed scope | `PermissionDenied` | Proposed |
| Other filesystem/read failure | `ExecutionFailed` | Proposed |

### 3.5. Validation/business rules

- Read UTF-8 text only. Image/binary bytes are read only through the internal Environment API for VisionAnalyzeTool.
- Normalize and scope-check the path; account for `..` and symlinks according to the policy still to be approved.
- NativeEnvironment is not a complete security sandbox; TOCTOU remains a limitation.
- Do not return partial file content on a read or UTF-8 error (**Proposed**).

Allowed roots, symlink behavior, path normalization, file-size limits, and exact filesystem error mapping remain Proposed/TBD.

### 3.6. Proposed tests

| Test | Expected outcome |
|---|---|
| Read valid UTF-8 text | Success with exact content |
| Read empty UTF-8 file | Success with empty `content` |
| Read invalid UTF-8 | `ExecutionFailed`; no partial content |
| Read missing file | Proposed `ResourceNotFound` |
| Read path outside allowed scope | Proposed `PermissionDenied` |
| Read path containing `..` or symlink | Result follows the still-to-be-approved path policy |

## 4. `write_file`

### 4.1. Purpose

Writes UTF-8 text using the Write operation of FileTool.

### 4.2. Arguments and proposed JSON Schema

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `path` | string | Yes | Destination path |
| `content` | string | Yes | UTF-8 text to write |
| `overwrite` | boolean | No | Whether an existing file may be replaced; Tool-level default is false |

```json
{
  "type": "object",
  "properties": {
    "path": {
      "type": "string"
    },
    "content": {
      "type": "string"
    },
    "overwrite": {
      "type": "boolean"
    }
  },
  "required": ["content", "path"],
  "additionalProperties": false
}
```

`overwrite=false` when omitted is a Tool implementation default, not a JSON Schema `default` keyword. The optional field and closed-object choice are Proposed.

### 4.3. Success `ToolResult.data`

```json
{
  "written": true
}
```

The success object shape is Proposed. It contains no file content.

### 4.4. Error mapping

| Condition | ErrorCode | Status |
|---|---|---|
| Destination exists and overwrite is false | `ExecutionFailed` | Proposed |
| Path is outside allowed scope | `PermissionDenied` | Proposed |
| Parent directory is missing | `ExecutionFailed` | Proposed |
| Other filesystem/write failure | `ExecutionFailed` | Proposed |

### 4.5. Validation/business rules

- Input content must be valid UTF-8 under Registry validation.
- Do not overwrite by default.
- Do not create missing parent directories automatically.
- Normalize and scope-check paths; handle `..` and symlinks under the policy still to be approved.
- NativeEnvironment is not a complete security sandbox; TOCTOU remains a limitation.
- Whether a failed/partial write is atomic and how concurrent overwrite races are handled remain TBD.

### 4.6. Proposed tests

| Test | Expected outcome |
|---|---|
| Write a new path with valid text | Success with `{"written":true}` |
| Omit `overwrite` for existing file | Proposed `ExecutionFailed`; original content unchanged |
| Set `overwrite=true` for existing file | Proposed success and new content |
| Parent directory does not exist | Proposed `ExecutionFailed`; no directory created |
| Destination outside allowed scope | Proposed `PermissionDenied` |
| Input content contains invalid UTF-8 | `InvalidArguments` from Registry validation |

## 5. `web_search`

### 5.1. Purpose

Searches information using the configured SearXNG backend. Search results are untrusted data and are not Agent instructions.

### 5.2. Arguments and proposed JSON Schema

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `query` | string | Yes | Search query |
| `max_results` | integer | No | Requested result count; Tool applies default 5 |

```json
{
  "type": "object",
  "properties": {
    "query": {
      "type": "string",
      "minLength": 1
    },
    "max_results": {
      "type": "integer",
      "minimum": 1,
      "maximum": 10
    }
  },
  "required": ["query"],
  "additionalProperties": false
}
```

Default 5, range 1–10, non-empty query, and closed-object behavior are Proposed. Default is applied in Tool code.

### 5.3. Success `ToolResult.data`

Proposed shape:

```json
{
  "results": [
    {
      "title": "Result title",
      "url": "https://example.invalid/",
      "snippet": "Short result excerpt"
    }
  ]
}
```

The result array preserves SearXNG order, is capped at the requested count, and each snippet is capped at 4,000 Unicode code points (**all Proposed**). The precise content truncation boundary and URL validation behavior are TBD.

### 5.4. Error mapping

| Condition | ErrorCode | Status |
|---|---|---|
| Empty query | `InvalidArguments` | Proposed |
| HTTP request timeout | `Timeout` | Proposed |
| SearXNG unavailable/transport failure | `ResourceUnavailable` | Proposed |
| SearXNG returns an unusable response | `ExecutionFailed` | Proposed |

### 5.5. Validation/business rules

- Endpoint is externally configured; do not hardcode a public instance.
- Configure SearXNG JSON output.
- Enforce proposed timeout, result-count, and per-snippet content limits.
- Treat result content as untrusted data, not instructions.
- HTTP request/response behavior follows the proposed HttpClient API contract; redirects, status handling, response-size limit, and timeout value remain TBD.

### 5.6. Proposed tests

| Test | Expected outcome |
|---|---|
| Mock SearXNG returns results | Success; result shape and backend order preserved |
| More than `max_results` results returned | At most requested number included |
| Snippet exceeds 4,000 code points | Proposed safe truncation |
| Empty query | Proposed `InvalidArguments` |
| HTTP timeout | Proposed `Timeout` |
| Backend unavailable | Proposed `ResourceUnavailable` |
| Backend returns malformed/unusable JSON | Proposed `ExecutionFailed` |
| Search result contains instruction-like text | Returned as untrusted result data; not executed as instruction |

Live integration test is conditional on a suitable environment. If not run, report `NOT RUN`, not `PASS`.

## 6. `memory_save`

### 6.1. Purpose

Saves a memory through MemoryTool and the injected MemoryStore abstraction. MemoryTool does not access SQLite directly.

### 6.2. Arguments and proposed JSON Schema

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `content` | string | Yes | Memory text |
| `tags` | array of string | No | Tags associated with the saved memory |

Assigning the approved `tags.maxItems=20` limit to `memory_save` is **Proposed**, not an existing approved ownership decision.

```json
{
  "type": "object",
  "properties": {
    "content": {
      "type": "string"
    },
    "tags": {
      "type": "array",
      "items": {
        "type": "string"
      },
      "minItems": 0,
      "maxItems": 20
    }
  },
  "required": ["content"],
  "additionalProperties": false
}
```

### 6.3. Success `ToolResult.data`

Proposed shape:

```json
{
  "memory_id": 42,
  "created": 1795699200
}
```

`memory_id` is int64 and `created` is a UTC Unix timestamp in seconds, as recorded in Overall Design. Returning these fields from `memory_save` is Proposed.

### 6.4. Error mapping

| Condition | ErrorCode | Status |
|---|---|---|
| Empty content | `InvalidArguments` | Proposed |
| More than 20 tags | `InvalidArguments` | Approved limit, proposed ownership/Tool mapping |
| MemoryStore/database unavailable | `ResourceUnavailable` | Proposed |
| Other storage failure | `ExecutionFailed` | Proposed |

Empty content and duplicate memory handling are not defined by the current approved contracts.

### 6.5. Validation/business rules

- Use MemoryStore; do not issue SQLite queries from MemoryTool.
- SQLite implementation uses parameterized SQL.
- Proposed default for omitted `tags` is an empty list, applied by Tool code.
- Duplicate content is not deduplicated in this proposal; uniqueness policy remains TBD.

### 6.6. Proposed tests

| Test | Expected outcome |
|---|---|
| Save content without tags | Proposed success with int64 ID and UTC seconds |
| Save content with tags | Proposed success; tags persist through MemoryStore |
| Save 20 tags | Schema accepts input |
| Save 21 tags | `InvalidArguments` |
| Empty content | Proposed `InvalidArguments` |
| MemoryStore failure | Proposed `ResourceUnavailable` |
| Save duplicate content | Behavior remains TBD until duplicate policy is approved |

## 7. `memory_search`

### 7.1. Purpose

Searches memories through MemoryTool and MemoryStore. MVP uses SQLite LIKE; FTS5 is not required.

### 7.2. Arguments and proposed JSON Schema

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `query` | string | Yes | Search text |
| `limit` | integer | No | Maximum records; Tool applies default 10 |

```json
{
  "type": "object",
  "properties": {
    "query": {
      "type": "string",
      "minLength": 1
    },
    "limit": {
      "type": "integer",
      "minimum": 1,
      "maximum": 50
    }
  },
  "required": ["query"],
  "additionalProperties": false
}
```

Non-empty query, default limit 10, maximum limit 50, and closed-object behavior are Proposed. The default is applied by Tool implementation, not JSON Schema.

### 7.3. Success `ToolResult.data`

Proposed shape:

```json
{
  "results": [
    {
      "memory_id": 42,
      "content": "Stored memory",
      "tags": ["example"],
      "created": 1795699200
    }
  ]
}
```

`memory_id` is int64 and `created` is UTC Unix timestamp seconds. Result order and whether `tags` is always present are Proposed.

### 7.4. Matching and error behavior

Proposed matching candidate:

- Literal substring search using SQLite LIKE.
- Escape `%`, `_`, and the chosen escape character so query text is literal rather than a wildcard pattern.
- Use SQLite's default LIKE case behavior: ASCII case-insensitive; non-ASCII case behavior follows SQLite build/configuration.
- Sort by `created` descending, then `memory_id` descending for stable ties.
- Return no more than `limit` records.

All matching semantics above are Proposed. Empty query → `InvalidArguments`; MemoryStore/database unavailable → `ResourceUnavailable`; other query failures → `ExecutionFailed` are Proposed mappings.

### 7.5. Proposed tests

| Test | Expected outcome |
|---|---|
| Query matches stored substring | Matching record appears |
| Query has no matches | Success with empty `results` |
| Query includes `%` or `_` | Proposed literal treatment; no wildcard expansion |
| ASCII case differs | Proposed SQLite LIKE behavior |
| Non-ASCII case differs | Follows selected SQLite behavior; exact expectation TBD |
| Equal timestamps | Proposed descending memory_id tie-break |
| Limit omitted | Proposed at most 10 records |
| Limit at 50 / above 50 | 50 accepted / above 50 rejected by schema |
| Empty query | Proposed `InvalidArguments` |
| Store unavailable | Proposed `ResourceUnavailable` |

## 8. `calculator`

### 8.1. Purpose

Evaluates a mathematical expression without `eval`.

### 8.2. Arguments and proposed JSON Schema

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `expression` | string | Yes | Arithmetic expression |

```json
{
  "type": "object",
  "properties": {
    "expression": {
      "type": "string",
      "minLength": 1,
      "maxLength": 256
    }
  },
  "required": ["expression"],
  "additionalProperties": false
}
```

Expression length 1–256 Unicode code points, field naming, and closed-object behavior are Proposed. Parser depth limit 64 is also Proposed.

### 8.3. Approved precedence and Proposed success shape

P3-D51 is Approved:

- `^` has higher precedence than unary `+` and `-`.
- `^` is right-associative.
- Unary operators are allowed on the exponent side.
- Required results: `-2^2 = -4`, `(-2)^2 = 4`, `2^3^2 = 512`, `2^-2 = 0.25`.

Consistent grammar:

```text
expression := additive
additive := multiplicative { ("+" | "-") multiplicative }
multiplicative := unary { ("*" | "/" | "%") unary }
unary := ("+" | "-") unary | power
power := primary [ "^" unary ]
primary := number | "(" expression ")"
```

Proposed success object:

```json
{
  "result": 12.5
}
```

The numeric representation is finite JSON number. Output field name is Proposed.

### 8.4. Errors and business rules

| Condition | ErrorCode | Status |
|---|---|---|
| Empty/malformed expression | `InvalidArguments` | Proposed |
| Division by zero | `InvalidArguments` | Proposed |
| Modulo operand is not an integer | `InvalidArguments` | Proposed |
| Result is NaN or Infinity | `ExecutionFailed` | Proposed |
| Expression exceeds parser depth | `InvalidArguments` | Proposed |

Support `+`, `-`, `*`, `/`, `%`, `^`, integer/floating-point numbers, unary `+`/`-`, and parentheses. Expression length and parser-depth values remain Proposed.

### 8.5. Proposed tests

| Input/condition | Expected outcome |
|---|---|
| `1+2*3` | Proposed success result 7 |
| `-2^2` | Approved success result -4 |
| `(-2)^2` | Approved success result 4 |
| `2^3^2` | Approved success result 512 |
| `2^-2` | Approved success result 0.25 |
| `1/0` | Proposed `InvalidArguments` |
| `5.5%2` | Proposed `InvalidArguments` |
| Malformed expression | Proposed `InvalidArguments` with `/expression` |
| Expression longer than 256 code points | Proposed `InvalidArguments` |
| Parser nesting exceeds 64 | Proposed `InvalidArguments` |
| Arithmetic produces non-finite value | Proposed `ExecutionFailed`; no non-JSON number returned |

## 9. `todo_list`

### 9.1. Purpose and operations

Manages an in-memory list using `add`, `list`, `complete`, and `remove`.

### 9.2. Arguments and proposed JSON Schema

| Field | Type | Required by schema | Meaning |
|---|---|---:|---|
| `operation` | string enum | Yes | `add`, `list`, `complete`, or `remove` |
| `text` | string | No | Task text; Tool requires it for `add` |
| `id` | integer | No | Task identifier; Tool requires it for `complete` and `remove` |

```json
{
  "type": "object",
  "properties": {
    "operation": {
      "type": "string",
      "enum": ["add", "list", "complete", "remove"]
    },
    "text": {
      "type": "string",
      "minLength": 1
    },
    "id": {
      "type": "integer",
      "minimum": 1
    }
  },
  "required": ["operation"],
  "additionalProperties": false
}
```

Field names, non-empty text, positive IDs, and closed-object behavior are Proposed. Operation-dependent requiredness is checked inside the Tool.

### 9.3. Capacity and ID policy

P3-D52 is Approved: at most 100 tasks may be stored.

Proposed behavior when adding task 101: reject the operation with `ResourceUnavailable` and leave the list unchanged. This error mapping and mutation guarantee are Proposed; P3-D52 approves the capacity, not this overflow behavior.

ID generation and reuse are **Proposed/TBD**. Candidate for review: assign positive IDs, never reuse IDs during the lifetime of the in-memory list, and use an int64 representation. No monotonically increasing or non-reusable ID policy is Approved by this document.

Operation rules proposed:

- `add`: require `text`; reject `id`; create an incomplete task if under capacity.
- `list`: reject `text` and `id`; return current tasks in insertion order.
- `complete`: require `id`; reject `text`; mark existing task complete. Whether repeated completion is idempotent is Proposed.
- `remove`: require `id`; reject `text`; remove existing task.
- Missing operation-specific fields or disallowed fields return `InvalidArguments`.
- Unknown task ID returns `ResourceNotFound`.

### 9.4. Proposed success structures

For `add` and `complete`:

```json
{
  "item": {
    "id": 1,
    "text": "Task",
    "completed": false
  }
}
```

For `list`:

```json
{
  "items": [
    {
      "id": 1,
      "text": "Task",
      "completed": false
    }
  ]
}
```

For `remove`:

```json
{
  "removed": true,
  "id": 1
}
```

All output field shapes and ordering are Proposed. `items` contains at most 100 tasks due to the Approved capacity.

### 9.5. Proposed tests

| Test | Expected outcome |
|---|---|
| Add with text | Proposed success with new incomplete item |
| List empty list | Proposed success with empty `items` |
| Complete existing ID | Proposed success with completed item |
| Remove existing ID | Proposed success with `removed=true` |
| Add without text | `InvalidArguments`, `/text` |
| Complete/remove without ID | `InvalidArguments`, `/id` |
| `list` includes text or ID | Proposed `InvalidArguments` |
| Complete/remove unknown ID | Proposed `ResourceNotFound` |
| Add the 100th task | Success; count is 100 |
| Add the 101st task | Proposed `ResourceUnavailable`; list remains at 100 and unchanged |
| Re-complete an already completed task | TBD pending idempotency choice |
| Remove then add | ID result follows the ID policy only after approval |

## 10. `vision_analyze`

### 10.1. Purpose

Analyzes an image using an injected LLMClient. It may use a model different from the Agent model and must not use Agent conversation history.

### 10.2. Arguments and proposed JSON Schema

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `image_path` | string | Yes | Path to image read through Environment's internal binary API |
| `prompt` | string | No | Optional analysis instruction |
| `model` | string | No | Optional model override; omission uses configured default |

```json
{
  "type": "object",
  "properties": {
    "image_path": {
      "type": "string"
    },
    "prompt": {
      "type": "string"
    },
    "model": {
      "type": "string"
    }
  },
  "required": ["image_path"],
  "additionalProperties": false
}
```

Field names, optional prompt/model, default model selection, and closed-object behavior are Proposed.

### 10.3. Image restrictions and success data

Proposed initial image constraints: accept PNG and JPEG, maximum 10 MiB. Both formats and size are Proposed, not Approved. Binary bytes are read internally through Environment and are not passed as JSON/base64 Tool arguments.

Proposed success object:

```json
{
  "analysis": "Model response"
}
```

The output field name is Proposed. Do not include Agent conversation history.

### 10.4. Error mapping

| Condition | ErrorCode | Status |
|---|---|---|
| Invalid image path/out-of-scope path | `PermissionDenied` or `ResourceNotFound` | Proposed; exact distinction TBD |
| Unsupported format or oversized image | `InvalidArguments` | Proposed |
| Selected model has no vision capability | `ResourceUnavailable` | Proposed |
| Model/service failure | `ResourceUnavailable` or `ExecutionFailed` | Proposed; mapping TBD |

### 10.5. Proposed tests

| Test | Expected outcome |
|---|---|
| Valid PNG within limit and vision-capable model | Proposed success with analysis |
| Valid JPEG within limit | Proposed success |
| Unsupported format | Proposed `InvalidArguments` |
| Image exceeds 10 MiB | Proposed `InvalidArguments` |
| Missing/out-of-scope path | Proposed mapping according to approved path policy |
| Model lacks vision capability | Proposed `ResourceUnavailable` |
| Agent history contains text | It is not sent to VisionAnalyzeTool's LLMClient call |
| LLM service fails | Proposed mapped error; exact code TBD |

## 11. `clarify`

### 11.1. Purpose

Requests user clarification through the UserInteraction abstraction. ClarifyTool does not call `cin` or `cout`.

### 11.2. Arguments and proposed JSON Schema

| Field | Type | Required | Meaning |
|---|---|---:|---|
| `question` | string | Yes | Question to present |
| `options` | array of string | No | Optional choices |

```json
{
  "type": "object",
  "properties": {
    "question": {
      "type": "string",
      "minLength": 1
    },
    "options": {
      "type": "array",
      "items": {
        "type": "string"
      },
      "minItems": 0,
      "maxItems": 10
    }
  },
  "required": ["question"],
  "additionalProperties": false
}
```

`options.maxItems=10` is Approved. Field names, non-empty question, optionality, and closed-object behavior are Proposed. An omitted options field means free-text interaction in the proposal; the Tool applies that behavior, not a JSON Schema default.

### 11.3. Proposed success `ToolResult.data`

Candidate shape:

```json
{
  "answer": "User response",
  "selected_option": null,
  "cancelled": false
}
```

The candidate uses `selected_option` as either the exact selected option string or `null` for free-text/cancel, `answer` as a string, and `cancelled` as a boolean. Cancellation could instead be represented by an empty answer or a distinct result; **these semantics are Proposed and require approval**, not an assumed contract.

### 11.4. Error mapping and interaction rules

| Condition | ErrorCode | Status |
|---|---|---|
| No interaction provider | `InteractionUnavailable` | Existing Overall Design contract |
| Invalid arguments | `InvalidArguments` | Approved Registry/schema flow |
| Provider timeout | `Timeout` or `InteractionUnavailable` | TBD |
| Provider throws unexpectedly | `InternalError` through Registry | Approved unexpected-exception flow |

Use UserInteraction only. Cancellation, timeout, empty response, option matching, and output null/empty semantics remain Proposed/TBD.

### 11.5. Proposed tests

| Test | Expected outcome |
|---|---|
| Provider returns free-text response | Proposed success with answer; selected-option semantics await approval |
| Provider selects a listed option | Proposed success with selected option |
| No options supplied | Proposed free-text interaction |
| No provider configured | `InteractionUnavailable` |
| Provider cancels | Result shape remains Proposed; assert only after cancellation semantics are approved |
| Provider returns empty text | Result shape remains Proposed; empty-answer semantics are not decided |
| Provider throws unexpectedly | Registry returns `InternalError`; raw exception details are not exposed |
| More than 10 options | `InvalidArguments` from schema validation |

## 12. Cross-Tool unresolved decisions

The following are not Approved by this draft:

- `tags` belongs to `memory_save`; only the `maxItems=20` value is already Approved. Ownership and field shape remain Proposed.
- Closed-object schemas (`additionalProperties:false`) for each Tool.
- Per-Tool success data field names unless specified as an existing Overall Design constraint.
- Proposed ErrorCode mappings for Tool business and infrastructure failures.
- Web Search defaults and numeric limits; SearXNG result/content behavior.
- Memory search default/max limits, wildcard escaping, case behavior, ordering, and duplicate handling.
- Calculator expression-length/parser-depth values and error mappings.
- TodoList overflow mapping, ID generation/reuse, and repeated-completion behavior.
- Vision image formats, image-size limit, model selection, and error mapping.
- Clarify cancellation, empty-answer, and selected-option/null semantics.
- Environment allowed roots, path/symlink behavior, process launch errors, HTTP redirects/timeouts/response-size limits, and concrete provider/storage failure mappings; see `phase-3-api-contracts.md`.

P3-D50 (UTF-8 process output), P3-D51 (calculator precedence), and P3-D52 (TodoList capacity) are Approved in the Overall Design. The overflow behavior and all other new technical details above remain Proposed or TBD. No Tool contract in this document is Approved as a whole.
