---
name: sane-foundation
description: Core Sane C++ primitives for Result, Span, StringSpan, Function, Deferred, opaque handles, and other base types. Use when integrating any Sane library, handling errors without exceptions, choosing non-owning views, or learning the public foundation API surface.
---

# Sane Foundation

## Overview

Use this skill to anchor every other Sane C++ question in the right base types. Teach the agent to start with `Result`, `Span`, `StringSpan`, `Function`, `Deferred`, and the handle wrappers before it reaches for higher-level libraries.

## Use This Skill When

- A request asks how to propagate errors without exceptions.
- A request asks which view type to use for bytes or strings.
- A request asks how to store callbacks with Sane-style capture limits.
- A request needs an OS handle wrapper or deferred cleanup.
- A request is about the common primitives shared by many Sane libraries.

## Start Here

- Read [references/foundation-entrypoints.md](references/foundation-entrypoints.md).
- Inspect `Libraries/Foundation/Result.h`, `Span.h`, `StringSpan.h`, `Function.h`, `Deferred.h`, `OpaqueObject.h`, and `UniqueHandle.h`.
- Use `Tests/Libraries/Foundation/*` as the behavior reference for these primitives.

## Key Guidance

- Prefer `SC::Result` for every fallible operation.
- Prefer non-owning spans and views when ownership is not required.
- Keep callbacks small enough for `SC::Function`.
- Use `Deferred` or the handle wrappers when the cleanup must be explicit and local.
- Route allocation-heavy or owned-string questions to `sane-memory`.

## Pitfalls

- Do not hide failures behind exceptions or implicit conversions.
- Do not capture large objects by value in callbacks.
- Do not assume this skill covers allocation or owned containers.
- Do not duplicate the higher-level guidance from other skills here.
