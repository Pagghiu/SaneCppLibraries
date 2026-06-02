# Sane Foundation

## Overview

Use this guide to anchor every other Sane C++ question in the right base types. Teach the agent to start with `Result`, `Span`, `StringSpan`, `Function`, `Deferred`, and the handle wrappers before it reaches for higher-level libraries.

## Use This Guide When

- A request asks how to propagate errors without exceptions.
- A request asks which view type to use for bytes or strings.
- A request asks how to store callbacks with Sane-style capture limits.
- A request needs an OS handle wrapper or deferred cleanup.
- A request is about the common primitives shared by many Sane libraries.

## Start Here

- Read [references/foundation-entrypoints.md](references/foundation-entrypoints.md).
- Inspect `Libraries/Common/Result.h`, `Span.h`, `StringSpan.h`, `Function.h`, `Deferred.h`, `OpaqueObject.h`, and `UniqueHandle.h`.
- Use `Tests/Libraries/Foundation/*` as the behavior reference for these primitives.

## Key Guidance

- Prefer `SC::Result` for every fallible operation.
- Prefer non-owning spans and views when ownership is not required.
- Keep callbacks small enough for `SC::Function`.
- Use `Deferred` or the handle wrappers when the cleanup must be explicit and local.
- Route allocation-heavy or owned-string questions to `memory`.

## Pitfalls

- Do not hide failures behind exceptions or implicit conversions.
- Do not capture large objects by value in callbacks.
- Do not assume this guide covers allocation or owned containers.
- Do not duplicate the higher-level guidance from other skills here.
