# OGK-Agent

A modular C++ AI agent framework using the Ollama API.

The project is developed incrementally, with clear boundaries between the
LLM Client, Tool System, Skill System, Agent Core, and Harness & Evaluation.

See [Architecture Overview](docs/architecture.md) for the approved
Architecture v1.0 boundaries and principles.

## Project Status

- [x] Repository and CMake Foundation
- [x] Phase 2: LLM Client & Infrastructure
- [ ] Phase 3: Tool System & Skill System
- [ ] Phase 4: Agent Core
- [ ] Phase 5: Harness & Evaluation
- [ ] Phase 6: Integration & Benchmark

### Current implementation

Phase 2 provides:

- Generic `LLMClient` interface.
- Ollama API implementation through `OllamaClient`.
- Request, response, message, and tool-calling data models.
- Configuration and error handling.
- Unit tests and HTTP integration tests.
- GitHub Actions CI workflow.

Tool execution, Skill System, AgentLoop, and Harness & Evaluation
are not implemented yet.

## Requirements

- CMake 3.20 or newer
- C++23-compatible compiler
- libcurl
- nlohmann/json 3.11.3 or newer
- Python 3 for the integration-test mock server

The project uses C++23 as its main C++ standard.
C++26 P2741R3 is an optional extension verified independently with Clang 18;
it does not change the project-wide standard.

## Build

Configure the project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

Build:

```sh
cmake --build build --parallel
```

## Run tests

CTest starts and stops the local mock Ollama server automatically:

```sh
ctest --test-dir build --output-on-failure
```

The test suite contains:

- `ogk_unit_tests`
- `ogk_integration_tests`

The integration tests use a mock HTTP server. They do not require a running
Ollama model. Python 3 is required to run the integration-test server.

## Documentation

- [Architecture](docs/architecture.md)
- [Phase 2 Interface Specification](docs/phase-2-interface-spec.md)
- [Phase 2 Handover](docs/phase-2-handover.md)
