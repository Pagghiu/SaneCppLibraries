# ASYNCEXECUTION-0008 - Borrowed File Options and Complete Transfers

Status: Accepted
Date: 2026-10-17

## Context

The prototype file factories require a pool and always select an absolute offset. Await exposes optional pool/offset options, while Fibers has At-style explicit offsets and an exact-read error contract. Await's fill-read instead succeeds at partial EOF. Native Windows requests also have their own cursor/append handling; exposing new options must not imply a portable shared descriptor cursor that the underlying adapters do not currently provide.

## Decision

Add AsyncExecutionFileOptions, with equivalent read/write aliases, carrying an optional borrowed pool, offset/useOffset and NativePreferred/ForceThreadPool mode. Default no-offset addressing delegates to the native request, matching the existing Await/Fibers delegation. It does not add a cross-platform shared-cursor guarantee. Explicit At factories select absolute offsets; use these for portable independent regular-file ranges. Keep legacy explicit-pool factories as absolute-offset compatibility calls. Do not alter unrelated adapters or silently emulate a new shared cursor.

One-shot reads and scalar/vector writes emit spans/EOF or byte counts and preserve native errors. Bound one-shot byte counts to the native 32-bit transfer representation. Validate explicit file ranges against the signed 64-bit offset domain before submitting. Complete writes iterate bounded contiguous chunks over scalar/vector data without mutating metadata. Provide actual fileWriteAll behavior rather than aliasing a one-shot request.

Offer separate fileReadExact and fileReadUntilFullOrEOF senders: both fill the requested span, but exact read errors on early EOF and fill-read succeeds with a partial span/EOF. Empty complete transfers succeed without I/O. Optional progress storage reports completed chunks on error/stopped; it remains borrowed through terminal completion. Reuse one native request and advance explicit offsets by observed bytes, with overflow checks before I/O.

Pool selection remains explicit. NativePreferred lets Async disable the attached pool on a native-file backend; ForceThreadPool requires a pool. Pipes/nonseekable handles normally omit both pool and offset. Once submitted to a worker, cancellation waits for completion and may not prevent queued I/O side effects. Buffers, descriptors, metadata and execution resources remain live until terminal completion.

## Consequences

The new surface matches existing control-flow adapters where contracts agree, while keeping exact-versus-EOF and native-addressing differences visible. Callers can select explicit portable file ranges without owning whole-file buffers. Non-template setup/progress/range logic stays in AsyncExecution.cpp; no new dependency between the adapters is introduced.

## Confirmation

Check explicit scalar/vector offsets, complete writes across chunk/vector boundaries, exact/fill EOF distinctions, zero progress, empty transfers, lazy/prestopped progress initialization, numeric bounds, forced/native-preferred pools and cancellation quiescence. Verify native pipe I/O independently of owning readFile/writeFile. Inspect Windows vector/cursor behavior rather than assuming POSIX behavior. Complete the file-group validation before advancing to UDP.
