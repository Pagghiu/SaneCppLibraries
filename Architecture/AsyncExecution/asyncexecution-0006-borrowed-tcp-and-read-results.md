# ASYNCEXECUTION-0006 - Borrowed TCP and Observable Transfer Progress

Status: Accepted
Date: 2026-10-17

## Context

Borrowed accept/connect, scatter/gather send and exact/line receive need ownership, progress and end-of-input contracts comparable to the other Async adapters. Matching names alone would conceal differences in socket ownership, partial error results and EOF handling.

## Decision

Expose accept(listener) as a lazy sender borrowing the listening socket and returning an owned AsyncExecutionConnection. Preserve native accept's loop association and explicitly configure the accepted descriptor as nonblocking before success. Do not require an output descriptor argument. Give the connection wrapper an empty state and nothrow move assignment so callers can retain it through runInto. Replaced descriptors use ordinary RAII close semantics; explicit close remains available when its failure must be observed. Connect(socket, SocketIPAddress/SocketAddress) borrows a caller-created nonblocking socket associated with the context's loop and sends no value on success. Descriptor creation/association remains explicit through AsyncEventLoop, matching the borrowed adapters.

Add send/sendAll overloads taking borrowed Span<Span<const char>> metadata. Borrow the array and payloads through completion; native scatter I/O may temporarily adjust and restore metadata, so callers cannot inspect it concurrently. One-shot send delegates the native scatter transfer, skips leading empty entries and preserves native error/partial-write behavior; an all-empty one-shot send fails with Async EmptyBuffer. Complete sendAll skips empty entries and retries partial writes in bounded contiguous chunks across the vector. Empty sendAll succeeds with zero bytes without submitting I/O. Successful values remain size_t; optional caller-owned byte-progress storage exposes completed chunks on error/stopped. Check total-size arithmetic before sending.

ReceiveExact fills the requested span, succeeds immediately for an empty span and fails on early disconnect or zero progress. ReceiveLine reads without read-ahead, consumes newline, removes a preceding carriage return only for a complete line, and returns partial EOF as successful incomplete input. Empty line storage is an error. A full line followed by newline fits; a further ordinary byte is consumed and produces an overflow error, preserving the stored prefix. This matches the inspected Await helper behavior. Fibers' absence of these helpers remains visible.

Successful receive values retain the borrowed data span and end/line flags. Optional progress output pointers expose already-completed bytes on failure/cancellation, since set_error(Result) cannot also carry a value. Reset progress on the owner thread when starting, never when constructing a lazy sender. Progress is not a concurrent polling API; its storage and borrowed buffers/descriptors/metadata must survive terminal completion. Original native errors retain their identity; new helper-specific errors belong to AsyncExecution. Cancellation still waits for native quiescence and never closes a caller-owned socket.

Retain AsyncExecutionReadResult for TCP and file reads: both expose a borrowed byte span and an end-of-input observation. Socket EOF means orderly peer closure; file EOF describes the current range/end and permits later growth. Separate nominal types would duplicate storage and consumer handling without enforcing ownership or adding data. Explain resource meaning at each factory; keep exact-read incomplete-input errors explicit.

## Consequences

Success remains value-oriented, while optional progress storage preserves partial-error visibility without changing the Result error channel. Bytewise line reading favors simple ownership and no read-ahead over throughput. Complete vector sends avoid native vector-count limits by iterating contiguous chunks; one-shot scatter sends retain native limits.

## Confirmation

Bounded loopback tests cover IP/general addresses, accept/connect with ownership transfer and runInto, ordered scatter/gather sends, deterministic short-transfer cursor advancement, empty entries, arithmetic checks, exact-read EOF/error progress, CRLF/line/EOF/overflow boundaries, cancellation and descriptor cleanup. Include value/stopped terminal destruction and concurrent composed transfers. Provide equivalent bounded callback/fiber/coroutine/sender echo examples, recording adapter gaps explicitly. Keep non-template work in AsyncExecution.cpp and verify required platform and build-boundary checks.
