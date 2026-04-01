# Global Sane C++ Rules

Use this reference when the user needs the cross-cutting Sane style, not a single library.

## Teach These Rules First

- Prefer `Result` over exceptions.
- Prefer views and spans over owned data when possible.
- Avoid `std::` types by default.
- Keep allocations explicit and caller-owned.
- Keep public headers free from system and compiler headers.

## Best Follow-Up Skills

- `foundation` for `Result`, `Span`, `Function`, `Deferred`, and handle wrappers.
- `memory` for buffers, allocators, and owned string storage.
- `strings` for conversions, formatting, and path helpers.
- `containers` for container choice and capacity tradeoffs.

## Good User Prompts To Route

- “Help me rewrite this STL-heavy code in Sane style.”
- “Which Sane type should I use for this callback or buffer?”
- “How do I keep this API allocation-aware?”
