---
name: sane-core-patterns
description: Core Sane C++ usage rules for third-party AI agents. Use when adapting STL-heavy or exception-heavy code to Sane style, choosing Result and Span patterns, or explaining the global no-STL, no-exceptions, no-hidden-allocation rules.
---

# Sane Core Patterns

## Quick Start

- Use this skill when the user needs Sane-wide coding rules more than one specific library.
- Start with [references/global-rules.md](references/global-rules.md).

## Core Rules

1. Prefer `Result` for fallible operations.
2. Prefer spans and views over owning storage when ownership is not needed.
3. Avoid `std::` and exceptions unless the user explicitly opts in.
4. Keep dependencies narrow and push ownership to the caller.

## When To Route Elsewhere

- Route buffer and allocator questions to `sane-memory`.
- Route string-formatting questions to `sane-strings`.
- Route callback and handle-wrapper questions to `sane-foundation`.
- Route container-selection questions to `sane-containers`.

## Pitfalls

- Do not replace one library-specific skill with generic C++ advice.
- Do not hide allocation or ownership decisions.
- Do not duplicate the detailed rules that belong in the specific library skills.

## References

- [references/global-rules.md](references/global-rules.md)
- `Documentation/Pages/Principles.md`
- `Documentation/Pages/BuildingUser.md`
