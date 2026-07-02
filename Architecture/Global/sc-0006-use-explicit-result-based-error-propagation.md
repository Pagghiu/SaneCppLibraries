# SC-0006 - Use Explicit Result-Based Error Propagation

Status: Accepted
Date: 2026-07-02

## Context

Sane C++ builds with exceptions disabled and keeps allocation and failure behavior visible. Constructors, destructors, hidden global state, and exception-style control flow make failures harder to expose consistently across operating systems and single-file builds.

## Decision

Fallible library operations return `SC::Result` or another explicit success/failure value. Callers propagate failures with the `SC_TRY` family where appropriate. When a fallible operation also needs to produce data, it writes to explicit caller-provided output parameters or storage.

## Consequences

APIs tend to use `init`, `create`, `assign`, `open`, or operation methods for fallible work instead of throwing from constructors. Error handling is noisier than exception-based code, but failures remain visible to agents, tests, and users. Destructors should not hide fallible shutdown semantics.

## Confirmation

A change preserves this decision when new fallible paths expose failure through `Result` or an equivalent explicit return value, returned results are checked or deliberately suppressed with the existing warning macros, and extra output is written through explicit caller-owned objects.

## Related

- [Coding style: Error checking](../../Documentation/Pages/CodingStyle.md#error-checking)
- [Foundation documentation](../../Documentation/Libraries/Foundation.md)
- [SC-0005 - Avoid STL, exceptions, and RTTI in library code](sc-0005-avoid-stl-exceptions-and-rtti-in-library-code.md)
