# Phase 2 Handover - LLM Client & Infrastructure

## 1. Phase status

Phase 2 is implemented and its Interface Specification v1.2 has been
approved and frozen.

This phase provides the LLM communication layer. It does not implement
AgentLoop, tool execution, or the Skill System.

## 2. Implemented modules

### Public interfaces and data models

Located in `include/ogk/llm/`:

- `llm_client.hpp`
- `ollama_client.hpp`
- `llm_errors.hpp`
- `message.hpp`
- `chat_options.hpp`
- `chat_request.hpp`
- `chat_response.hpp`
- `tool_call.hpp`
- `tool_definition.hpp`

### Source implementation

Located in `src/llm/`:

- `llm_errors.cpp`
- `ollama_client.cpp`

### Tests

- `tests/unit_tests.cpp`
- `tests/ollama_client_integration_tests.cpp`
- `tests/mock_server.py`

## 3. Dependencies and build

The project uses:

- C++23
- CMake 3.20 or newer
- libcurl
- nlohmann/json
- Python 3 for the mock integration server

Configure:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
```

Build:

```sh
cmake --build build --parallel
```

Start the mock server in a separate terminal:

```sh
python3 tests/mock_server.py
```

Run tests:

```sh
ctest --test-dir build --output-on-failure
```

The integration tests expect the mock server at `127.0.0.1:8000`.

## 4. Test coverage

The current test suites are:

- `ogk_unit_tests`
- `ogk_integration_tests`

The integration coverage includes HTTP communication, request/response
serialization, HTTP 500 responses, malformed JSON, invalid response
structures, connection refusal, and request timeout behavior.

The tests use a mock HTTP server. They do not establish that a real Ollama
model or the Kaggle-hosted model has been successfully integrated.

The GitHub Actions workflow builds the project and runs CTest on Ubuntu
24.04.

## 5. Known limitations and follow-up considerations

- Tool calling is represented at the data-model/API level. Tool execution
  belongs to a later phase.
- Environment is design-only in Phase 2. Native and Sandbox implementations
  are deferred.
- The current response model is text-oriented; image input support depends
  on the backend and model capabilities.
- Real-model integration and model-specific behavior still need to be
  evaluated in the later integration phase.
- Timeout classification should be reviewed in future testing. The current
  implementation uses a timing-based distinction; this should not be
  interpreted as exhaustive proof of every connection-timeout scenario.

## 6. Architecture constraints for the next phase

The next phase is Phase 3 - Tool System & Skill System.

Implementation must preserve these constraints:

- Tool implementations must not depend on AgentLoop.
- AgentLoop must not know that Harness exists.
- Keep the LLM Client generic and independent of a specific backend.
- Do not introduce dependencies without a concrete requirement.
- Do not change frozen public interfaces without explicit approval.
- Environment implementations are deferred; do not add them as part of
  Phase 2.

## 7. Handover checklist

- [x] Phase 2 interface specification approved.
- [x] LLM Client interfaces implemented.
- [x] Ollama client implementation added.
- [x] Unit tests added.
- [x] HTTP integration tests added.
- [x] CI workflow added.
- [x] Phase 2 documentation updated.
- [ ] Merge Phase 2 branch into main after final verification.

## 8. Reference documents

- `docs/architecture.md`
- `docs/phase-2-interface-spec.md`
- `README.md`
