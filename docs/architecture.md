# Architecture v1.0

## Project purpose and architectural goals

OGK-Agent is a C++ AI agent framework using the Ollama API.
Architecture v1.0 establishes the boundaries for a modular agent system
while keeping the design simple and avoiding functionality or dependencies
before they are needed.

## System boundaries and major components

The approved design identifies the following conceptual components:

- **AgentLoop** coordinates agent execution through hook interfaces.
  It must not know that Harness exists.
- **Harness** provides a boundary for integration and evaluation work.
  Its detailed responsibilities and interfaces are TBD.
- **Tool implementations** provide tools and must not depend on AgentLoop.
- **Evaluators** assess agent behavior and remain independent of the Agent
  execution mechanism.
- **LLMClient** provides a generic client boundary. Another backend can be
  introduced through another implementation.

The Phase 2 LLM Client interfaces are specified separately in
`docs/phase-2-interface-spec.md`. That specification does not change the
frozen component boundaries of Architecture v1.0.

The exact interfaces of components outside the Phase 2 LLM Client scope
remain TBD.

## Component responsibilities and dependency constraints

AgentLoop exposes hook interfaces without depending on Harness.
Tool implementations do not depend on AgentLoop.
Evaluators do not depend on the Agent execution mechanism.
LLMClient is generic so backend implementations can vary.

Other dependency relationships and component-level interface details
remain TBD until their respective phases.

## Approved architectural principles

- Architecture v1.0 is frozen; public architectural boundaries must not be
  changed without explicit approval.
- Follow RAII and modern C++ practices.
- Keep the design simple and follow YAGNI.
- Do not add dependencies before an actual module requires them; prefer
  `find_package()` when one is genuinely needed.
- Use C++23 for the main project. C++26 P2741R3 is optional and independently
  verified with Clang 18; it must not affect the main project standard.
- Use CMake 3.20 or newer, an `ogk_agent_options` INTERFACE target for shared
  compiler settings, and CTest for test infrastructure.
- Create the `ogk_agent` library when actual source implementation exists.
- Keep testing and CI configuration proportional to the implemented modules.
- Do not create empty placeholder directories.

## Current implementation status

### Completed

**Foundation**
- CMake configuration.
- Shared `ogk_agent_options` INTERFACE target.
- CTest integration.
- GitHub Actions CI workflow.

**Phase 2 - LLM Client & Infrastructure**
- Generic `LLMClient` interface.
- `OllamaClient` implementation.
- Request, response, message, and tool-calling data models.
- Error types and request validation.
- Unit and HTTP integration tests.

The implementation is organized under:
- `include/ogk/llm/`
- `src/llm/`
- `tests/`

The main library target is `ogk_agent`.

### Not implemented yet

- Tool System.
- Skill System.
- Agent Core and AgentLoop.
- Harness & Evaluation.
- Full benchmark and final integration.

These components remain future work and must follow the frozen
architectural boundaries.
