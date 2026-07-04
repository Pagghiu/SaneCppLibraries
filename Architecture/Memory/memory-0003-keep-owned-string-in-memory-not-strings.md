# MEMORY-0003 - Keep Owned String in Memory, Not Strings

Status: Accepted
Date: 2026-07-04

## Context

An owned string looks like it belongs in `Strings`, but `String` and `SmallString` are allocation-capable storage types backed by `Buffer` and allocator policy. Keeping them in `Strings` would make string parsing, formatting, and path manipulation pull allocation-capable code into otherwise allocation-free libraries.

## Decision

`String` and `SmallString` live in `Memory`. They expose string content through `StringSpan` views and can be used as output targets through `GrowableBuffer` specializations, but their ownership and allocation behavior remain part of the `Memory` library.

## Consequences

`Strings` can remain focused on non-owning views, builders, formatting, conversion, and path algorithms without depending on `Memory`. Users who want an owned Sane string opt into the allocation-capable library explicitly. This is less conventional than placing all string types together, but it keeps dependency and allocation semantics visible.

## Confirmation

A change preserves this decision when owned Sane string storage remains under `Libraries/Memory`, `Strings` does not depend on `Memory` for normal string algorithms, and APIs that only need a string view use `StringSpan` or `StringView` instead of requiring `String`.

## Related

- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [MEMORY-0001 - Keep Memory as the explicit allocation boundary](memory-0001-keep-memory-as-the-explicit-allocation-boundary.md)
- [STRINGS-0001 - Keep Strings allocation-free and output through GrowableBuffer](../Strings/strings-0001-keep-strings-allocation-free-and-output-through-growablebuffer.md)
- [String](../../Libraries/Memory/String.h)
- [Strings documentation](../../Documentation/Libraries/Strings.md)
