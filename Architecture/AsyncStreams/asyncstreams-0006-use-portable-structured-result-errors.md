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

Transform state errors are local AsyncStreams identities. A failure returned by a transform's `onFinalize` callback
remains the original `Result`, even if owned by another category, rather than being replaced with generic text. The
failed finalize path releases its acquired output buffer so the caller-owned pool remains reusable.

Primary codes describe portable operations and failure conditions. A zlib status or operating-system-specific detail
may be carried later in a bounded library-specific enriched result, but must not become a platform-specific primary
code without an explicit ADR justification. The formatter remains optional so unused English messages can be removed
by the linker.

Compression runtime discovery uses the same `CompressionRuntimeUnavailable` primary code for `dlopen`, `LoadLibrary`,
and the Windows registry fallback. A loaded runtime missing a required function uses `CompressionSymbolMissing`.
Registry stage and native-loader details are intentionally not promoted into OS-specific primary codes; if callers
need them, they require a separately designed bounded extension rather than borrowed text in plain `Result`.
Native zlib statuses become portable operation outcomes such as invalid compressed data, missing dictionary, runtime
version mismatch, or no progress. A normal `STREAM_END` from decompression or finalization remains success with the
existing `streamEnded` output; only an unexpected end during compression processing is an error.
Uninitialized zlib stream operations report `CompressionNotInitialized` before using runtime function pointers.
Failed initialization releases its runtime reference immediately, and destruction calls native cleanup only for a
successfully initialized stream. Both synchronous and asynchronous zlib adapters forward these structured failures
without replacing them with generic text.

The header-only Async request adapters classify their own missing event-loop and descriptor preconditions, while
forwarding errors from Async requests unchanged. Event-loop absence is checked before acquiring a pool buffer or
changing callback state, so it cannot strand pool capacity. A simulated Async request failure is tested with a foreign
numeric category/value rather than a legacy message-pointer identity.
The template-only cipher adapter uses local codes for an output buffer smaller than one block and for a session that
reports more bytes than the supplied output span can hold. Errors returned by the cipher session itself retain their
foreign identity, and the adapter still has no Cryptography library dependency.

## Consequences

Callers of the existing `Result` APIs can continue using `SC_TRY` unchanged. Callers that need to distinguish
AsyncStreams failures inspect category and value, and callers that need text opt into formatting. No AsyncStreams
producer or consumer relies on the removed legacy text bridge.

## Confirmation

Buffer-pool tests assert stable category/value, zero-slice and overflowing child-view rejection, and optional formatter
behavior. Pipeline tests distinguish missing components and reject a duplex transform with only one matching pool.
The category-registry validator, single-file compilation, and platform Debug/Release suites are promotion
gates for the completed library port.

## Related

- [Common structured Result decision](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [AsyncStreams architecture](asyncstreams-architecture.md)
