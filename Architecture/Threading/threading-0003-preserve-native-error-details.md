# THREADING-0003 - Preserve Native Error Details

Status: Accepted
Date: 2026-09-12

## Context

Portable Threading error codes let callers identify the failed operation, but native thread creation, join, and detach
failures often require the platform error number for diagnosis. Putting that number in plain `Result` would charge
every library for Threading-specific context, while keeping only canonical prose would lose machine-readable detail.

## Decision

Threading APIs return the composed `ResultThreading` type. Its authoritative `Result` member carries the stable
Threading category and error code, while `nativeError` optionally carries the copied POSIX error value or Windows
`GetLastError()` value. Zero means unavailable or irrelevant. The type owns no memory, allocates nothing, remains
standard-layout and trivially copyable, and will be 16 bytes after the temporary Result migration bridge is removed.

Conversion to plain `Result` is implicit and deliberately discards `nativeError`. Conversion to `bool` is explicit so
generic overload sets cannot become ambiguous between boolean and plain-result consumers; contextual boolean checks
remain supported. Same-domain `SC_TRY` propagation retains the complete `ResultThreading`, while construction from a
plain or foreign result preserves its error identity and clears native context.

The opt-in Threading formatter appends a labelled decimal native error number when it is available. Applications may
instead consume the public fields and produce translated diagnostics.

## Consequences

Callers that retain the enriched type can report actionable native diagnostics, while existing code that stores or
returns plain `Result` continues to compile and intentionally loses only optional detail. Changing the exported return
type is part of the already-required Result ABI transition. During migration, the legacy pointer temporarily expands
`ResultThreading` to 24 bytes on 64-bit targets.

## Confirmation

Tests verify layout properties, local and foreign identity behavior, enriched-to-plain conversion, native-context
formatting, exact sizing, and translated numeric formatting. Thread creation paths capture native codes at the failure
site, and Debug/Release builds pass on each supported platform.
