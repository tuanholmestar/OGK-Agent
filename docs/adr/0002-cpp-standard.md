# ADR 0002: C++ Standard

## Status

Accepted

## Context

The project needs a main language standard and a way to record an independently
verified newer-language extension without changing that standard.

## Decision

- C++23 is the main project standard.
- C++26 P2741R3 is an optional extension, independently verified with Clang 18.
- The optional extension must not affect the main project standard.

## Consequences

Main project code and shared compiler settings target C++23. Use of C++26
P2741R3 remains optional and separate from that project-wide setting.
