# Getting Started

Use this page before reading any topic-specific guide.

## Repo-Wide Rules

- Sane library code must avoid dynamic allocation unless the API explicitly pushes storage to the caller.
- Do not use STL, exceptions, or RTTI in library code.
- Prefer `SC::` types and `SC::Result`-style error propagation.
- Do not include system headers in public `.h` files.
- Keep dependencies narrow and follow existing library boundaries.

## Default Reading Order

1. Read [core-patterns/guide.md](core-patterns/guide.md) for the global style and adaptation rules.
2. Read one primary topic guide for the library or workflow the user actually needs.
3. Read a companion guide only if the primary guide explicitly depends on it.
4. Inspect the referenced docs, headers, tests, examples, or tools in the repo before answering.

## Choosing The First Guide

- Integration or "where do I start?": [adoption/guide.md](adoption/guide.md)
- "Show me the best example or test": [examples/guide.md](examples/guide.md)
- Base types like `Result`, `Span`, `Function`, handles: [foundation/guide.md](foundation/guide.md)
- Memory ownership, buffers, allocators: [memory/guide.md](memory/guide.md)
- Containers, strings, time, or threading: [topic-map.md](topic-map.md)
- Async, networking, files, processes, or serial ports: [topic-map.md](topic-map.md)
- Reflection, serialization, plugins, or build/tooling: [topic-map.md](topic-map.md)

## Answering Style

- Prefer the smallest relevant set of repo paths.
- Use tests and examples as behavior references.
- Avoid replacing repo-specific patterns with generic C++ abstractions.
- When a workflow spans multiple libraries, explain the composition boundary clearly.
