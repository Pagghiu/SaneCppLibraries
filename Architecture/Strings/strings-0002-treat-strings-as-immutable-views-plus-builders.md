# STRINGS-0002 - Treat Strings as Immutable Views Plus Builders

Status: Accepted
Date: 2026-07-04

## Context

Mutable string APIs tend to combine parsing, ownership, growth, and editing policy in one type. That would make `Strings` harder to keep allocation-free and would blur the difference between inspecting existing text and producing new text.

## Decision

`Strings` treats strings as immutable views plus explicit builders. `StringView` provides read-only inspection, slicing, tokenization, parsing, and comparison. Operations that would modify text instead write new output through `StringBuilder`, `StringFormat`, `StringConverter`, `Path`, or another caller-provided output path.

## Consequences

String algorithms are easier to use without ownership or allocation assumptions. Callers must build replacement output explicitly rather than mutating a string in place. Builder lifetimes and finalization become part of the interface contract for output-producing operations.

## Confirmation

A change preserves this decision when `StringView` remains non-owning and read-only, mutation-style functionality produces output through builder or converter APIs, and tests exercise builder finalization and view lifetimes instead of relying on mutable string state.

## Related

- [STRINGS-0001 - Keep Strings allocation-free and output through GrowableBuffer](strings-0001-keep-strings-allocation-free-and-output-through-growablebuffer.md)
- [Strings documentation](../../Documentation/Libraries/Strings.md)
- [StringView](../../Libraries/Strings/StringView.h)
- [StringBuilder](../../Libraries/Strings/StringBuilder.h)
- [StringFormat](../../Libraries/Strings/StringFormat.h)
