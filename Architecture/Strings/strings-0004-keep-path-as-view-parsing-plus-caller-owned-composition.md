# STRINGS-0004 - Keep Path as View Parsing Plus Caller-owned Composition

Status: Accepted
Date: 2026-07-04

## Context

Path handling needs parsing, normalization, relative-path calculation, joining, native separators, and Windows/POSIX differences. It is tempting to model paths as owned strings, but doing so would make ordinary path algorithms depend on allocation-capable storage and would duplicate the fixed native path role already handled by `StringPath`.

## Decision

`Path` is a view parser and caller-owned composer. Parsing APIs return `StringView` slices into caller-provided input. Composition APIs such as join, normalize, append, and relative-path calculation write into caller-provided output through growable-buffer adapters and explicit encoding.

## Consequences

Path algorithms stay allocation-free and usable with fixed buffers, `StringPath`, `Memory::String`, or external containers. Callers must ensure input views outlive parsed views and must provide scratch component storage for operations that need it. Fixed native path storage remains a Common primitive rather than becoming part of `Strings`.

## Confirmation

A change preserves this decision when path parsing returns views instead of owned strings, path composition takes caller-owned output, normalization and relative-path operations avoid hidden allocation, and low-level path consumers can use `StringPath` without depending on `Strings`.

## Related

- [COMMON-0008 - Keep StringSpan and StringPath in Common](../Common/common-0008-keep-stringspan-and-stringpath-in-common.md)
- [STRINGS-0001 - Keep Strings allocation-free and output through GrowableBuffer](strings-0001-keep-strings-allocation-free-and-output-through-growablebuffer.md)
- [Path](../../Libraries/Strings/Path.h)
- [StringPath](../../Libraries/Common/StringPath.h)
- [Strings documentation](../../Documentation/Libraries/Strings.md)
