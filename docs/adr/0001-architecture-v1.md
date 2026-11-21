# ADR 0001: Architecture v1.0

## Status

Accepted (frozen)

## Context

OGK-Agent needs stable boundaries for its agent execution, LLM integration,
tools, and evaluation as implementation begins. Architecture v1.0 records the
approved structure and dependency constraints. Exact interface specifications
and a component diagram are TBD.

## Decision

- AgentLoop exposes hook interfaces and must not know that Harness exists.
- Tool implementations must not depend on AgentLoop.
- Evaluators remain independent of the Agent execution mechanism.
- LLMClient is generic so another backend can be introduced through another
  implementation.
- Follow RAII and modern C++ practices, keep the design simple, and follow YAGNI.
- Do not change frozen public interfaces.

## Consequences

Implementations must preserve the approved boundaries and avoid assumptions
about unspecified interfaces or ownership. Details not present in the approved
architecture remain TBD rather than being inferred during documentation.
