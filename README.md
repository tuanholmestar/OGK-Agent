# OGK-Agent

C++ AI agent framework using the Ollama API. The project is being developed as a
small, modular foundation that can support agent execution, tools, and evaluation.

See the [architecture overview](docs/architecture.md) for the approved Architecture
v1.0 boundaries and principles.

## Project Status

- [x] Repository initialized
- [x] Foundation (build configuration and documentation)
- [ ] LLM Client
- [ ] Tool System
- [ ] Skill System
- [ ] Agent Core
- [ ] Harness & Evaluation

The repository currently contains Foundation configuration and documentation,
but no implemented application modules or tests. CTest is selected as the test
infrastructure, but there are no tests to run at this stage.

## Verified toolchain

- GCC 13.3.0
- Clang 18.1.3
- CMake 3.28.3

The project requires CMake 3.20 or newer and uses C++23 as its main C++ standard.
C++26 P2741R3 is an optional extension verified independently with Clang 18; it
does not change the project-wide standard.

## Configure

From the repository root, configure with:

```sh
cmake -S . -B build
```

This configures the current Foundation build. It does not imply that an
application target can be built or that tests exist.
