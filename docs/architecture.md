# Architecture v1.0

## Project purpose and architectural goals

OGK-Agent is a C++ AI agent framework using the Ollama API. Architecture v1.0
sets the boundaries for a modular agent system while keeping the design simple
and avoiding functionality or dependencies before they are needed.

## System boundaries and major components

The approved design identifies the following conceptual components:

- **AgentLoop** coordinates agent execution through hook interfaces. It must not
  know that Harness exists.
- **Harness** provides a boundary for integration and evaluation work; its
  detailed responsibilities and interfaces are TBD.
- **Tool implementations** provide tools and must not depend on AgentLoop.
- **Evaluators** assess agent behavior and remain independent of the Agent
  execution mechanism.
- **LLMClient** provides a generic client boundary. Another backend can be
  introduced through another implementation.

The exact public interface specifications and an approved component diagram are
TBD. No class members, method signatures, or ownership details are specified
here.

## Component responsibilities and dependency constraints

AgentLoop exposes hook interfaces without depending on Harness. Tool
implementations do not depend on AgentLoop. Evaluators do not depend on the
Agent execution mechanism. LLMClient is generic so backend implementations can
vary. Other dependency relationships and component-level interface details are
TBD.

## Approved architectural principles

- Architecture v1.0 is frozen; public interfaces must not be changed as part of
  implementation work.
- Follow RAII and modern C++ practices.
- Keep the design simple and follow YAGNI.
- Do not add dependencies before an actual module requires them; prefer
  `find_package()` when one is genuinely needed.
- Use C++23 for the main project. C++26 P2741R3 is optional and independently
  verified with Clang 18; it must not affect the main project standard.
- Use CMake 3.20 or newer, an `ogk_agent_options` INTERFACE target for shared
  compiler settings, and CTest for test infrastructure.
- Do not create the `ogk_agent` library until actual source implementation
  exists.
- Do not add a third-party test framework, CMake presets, CI, Sanitizer, or
  Coverage configuration at this stage.
- Do not create empty placeholder directories.

## Current implementation status

The repository has a CMake Foundation configuration, including the shared
`ogk_agent_options` target and CTest inclusion. It has no implemented
application modules or tests, and no `ogk_agent` library target. The LLM Client,
Tool System, Skill System, Agent Core, and Harness & Evaluation remain future
work.
