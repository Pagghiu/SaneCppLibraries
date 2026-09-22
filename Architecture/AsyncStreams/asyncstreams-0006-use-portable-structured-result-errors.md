# ASYNCSTREAMS-0006 - Use Portable Structured Result Errors

Status: Accepted
Date: 2026-09-25

## Context

AsyncStreams currently reports failures with English string literals, including implementation-specific descriptions of
buffer-pool state, stream state, zlib status, and adapter preconditions. Those strings do not provide stable machine
identity and can be retained in binaries even when a caller never formats an error. Streams can also propagate failures
from Async, Cryptography, caller callbacks, and other libraries; reclassifying those errors would lose their origin.

## Decision

AsyncStreams owns category 16 and an append-only portable `AsyncStreamsError` enum. Library-owned producers return
that category and a code. Errors returned by another library or caller code pass through unchanged. The mandatory
`AsyncStreamsError.h` contains no English error text; the optional `AsyncStreamsErrorFormatter.h` formats only when a
caller asks. Plain `Result` remains the stored callback/result type and does not own text or allocate memory.

The first ports cover buffer-pool, readable-stream, and writable-stream failures. The same `InvalidBufferID`, `InvalidParentBufferID`, and `BufferPoolFull`
codes apply regardless of the operation that observes them. `NoReusableBuffer` distinguishes temporary inability to
satisfy a request from a full pool with no free slot. An invalid zero slice count is reported before division, and
child-view bounds are checked without unsigned addition overflow. Readable failures distinguish missing queue storage,
an invalid stream state, queue saturation, and missing reactivation after synchronous data delivery. Writable failures
distinguish queue saturation from a write attempted after end. Pipeline setup distinguishes missing source/sink,
listener capacity, and a buffer-pool mismatch. Both sides of a duplex transform must use the pipeline pool; the
previous check accepted a transform when only one side matched. Subsequent ports add compression,
and adapter codes to this single library-owned category.

Primary codes describe portable operations and failure conditions. A zlib status or operating-system-specific detail
may be carried later in a bounded library-specific enriched result, but must not become a platform-specific primary
code without an explicit ADR justification. The formatter remains optional so unused English messages can be removed
by the linker.

## Consequences

Callers of the existing `Result` APIs can continue using `SC_TRY` unchanged. Callers that need to distinguish
AsyncStreams failures inspect category and value, and callers that need text opt into formatting. During migration,
other AsyncStreams producers may still use the temporary legacy bridge; it is removed only after a complete producer
and consumer audit.

## Confirmation

Buffer-pool tests assert stable category/value, zero-slice and overflowing child-view rejection, and optional formatter
behavior. Pipeline tests distinguish missing components and reject a duplex transform with only one matching pool.
The category-registry validator, single-file compilation, and platform Debug/Release suites are promotion
gates for the completed library port.

## Related

- [Common structured Result decision](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [AsyncStreams architecture](asyncstreams-architecture.md)
