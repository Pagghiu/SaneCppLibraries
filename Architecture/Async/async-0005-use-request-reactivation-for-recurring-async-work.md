# ASYNC-0005 - Use Request Reactivation For Recurring Async Work

Status: Accepted
Date: 2026-07-04

## Context

Some async operations are naturally recurring: timeouts may be periodic, accepts may continue listening, wake-ups may remain armed, and stream-like reads may continue until EOF. Automatically hiding recurrence behind separate objects would obscure the active request lifetime.

## Decision

Recurring async work uses explicit request reactivation. A completion callback calls `AsyncResult::reactivateRequest(true)` when the same request should remain active for another operation. If the callback does not reactivate the request, the event loop completes teardown and returns the request to the free state.

## Consequences

Callers keep direct control over whether a request is one-shot or recurring. This makes active lifetime visible but requires callbacks to update offsets, buffers, counters, or policy before requesting reactivation. Higher-level wrappers can build loops on top of this primitive without changing the underlying request contract.

## Confirmation

A change preserves this decision when periodic or continuing operations still opt into recurrence from the completion path, reactivated requests remain memory-stable, and tests cover both one-shot completion and callback-driven reactivation.

## Related

- [AsyncResult::reactivateRequest](../../Libraries/Async/Async.h)
- [ASYNC-0001 - Keep AsyncRequest objects caller-owned and memory-stable](async-0001-keep-asyncrequest-objects-caller-owned-and-memory-stable.md)
- [Async documentation](../../Documentation/Libraries/Async.md)
