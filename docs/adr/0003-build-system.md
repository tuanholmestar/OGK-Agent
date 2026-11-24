# ADR 0003: Build System and Foundation Configuration

## Status

Accepted

## Context

The repository needs a minimal build foundation before application modules
exist. The approved toolchain verification covers GCC 13.3.0, Clang 18.1.3,
and CMake 3.28.3.

## Decision

- Use CMake with a minimum version of 3.20.
- Use an `ogk_agent_options` INTERFACE target for shared compiler settings.
- Do not create the `ogk_agent` library until actual source implementation
  exists.
- Use CTest as the test infrastructure; do not add a third-party test framework
  at this stage.
- Do not add dependencies before an actual module requires them. Prefer
  `find_package()` when a dependency is genuinely needed.
- Do not add `CMakePresets.json`, CI, Sanitizer, or Coverage configuration at
  this stage.
- Do not create empty placeholder directories.

## Consequences

The Foundation remains a minimal CMake configuration. Configure it from the
repository root with `cmake -S . -B build`. Configuration does not assert that
an application target or tests exist. Add build targets, dependencies, and
additional infrastructure only when implementation requires them.
