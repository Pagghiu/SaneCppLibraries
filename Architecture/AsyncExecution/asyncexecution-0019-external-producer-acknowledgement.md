# ASYNCEXECUTION-0019 - External Producer Acknowledgement

Status: Accepted
Date: 2026-10-19

## Context

A third-party callback can finish on an arbitrary thread while its buffers belong to a sender graph. Native manual AsyncExternalCompletion requires an active request before posting. Its manual cancellation clears submissionPending without joining an arbitrary external producer. Simply wrapping it in the generic stoppable operation would therefore give an unsupported buffer-quiescence guarantee.

## Decision

Add nonmovable AsyncExecutionExternalCompletion, bound to one borrowing context, and context.externalCompletion(record), a lazy sender<size_t>. The record is a single-use acknowledgement for an external job, not a subscription or task launcher. A producer calls record.complete(Result, bytes) only after its last access to graph-borrowed data. Completion before the wait starts is retained. Preserve the producer Result exactly; bytes are a success value. A successful second complete reports native AsyncError::CompletionAlreadyPosted. If publication fails, the first complete returns that failure without committing the record, permitting a retry after quiescence.

One active wait is allowed. Reject overlap, a different context and a second consumption with distinct AsyncExecution errors. Source state and its intrusive command are protected by a mutex. Delivery snapshots status/bytes, unlinks the command and detaches the waiter before invoking a terminal receiver, which may free connected storage. The record and context remain borrowed through all producer calls; join producers before destroying them.

Stop is a request to the producer, not permission to abandon it. Register the receiver stop token after accepting its wait; set an atomic isStopRequested flag that cooperative producers may poll. Even prestop waits for acknowledgement, because the external job may already be running. After acknowledgement, producer error wins over stop; otherwise emit stopped or the byte count. A timeout around this sender likewise joins acknowledgement before exposing its result. A producer that never acknowledges cannot be forcibly joined by this library.

Use the existing owner command dispatcher and native AsyncLoopWakeUp to marshal portable callback completion. Do not fabricate native request state, add an activation polling timer, block a worker or assume IOCP cancellation joins an unrelated job. This is the portable manual-completion bridge. Direct submission of Windows native OVERLAPPED operations is not supplied by this API and remains a separate capability. No new allocation, provider algorithm or dependency is required. Non-template state and publication logic belong in AsyncExecution.cpp.

## Confirmation

Exercise completion before/after wait, success bytes and exact producer error identity, duplicate publication/consumption, overlap/context mismatch, already-stopped and active stopped waits, acknowledgement after stop, producer polling and loop progress during the wait. Verify close rejects a pending acknowledgement, native/context error storage destruction and terminal heap-storage release. Run bounded Debug/Release suites on all available platforms, plus metadata, documentation and enabled/disabled integration boundaries.
