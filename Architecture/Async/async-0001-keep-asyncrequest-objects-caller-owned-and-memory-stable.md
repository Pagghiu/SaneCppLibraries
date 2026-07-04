# ASYNC-0001 - Keep AsyncRequest Objects Caller-Owned And Memory-Stable

Status: Accepted
Date: 2026-07-04

## Context

Async operations need per-request state while the operating system, event loop, cancellation path, or completion callback may still reference the operation. Hidden allocation would obscure that lifetime and conflict with the project's no-hidden-allocation rule.

## Decision

`AsyncRequest`-derived objects are caller-owned operation state. The event loop may link, activate, cancel, complete, and mark those objects free, but it does not allocate, copy, or take ownership of them. Callers must keep each request object's address stable until the request has completed, been stopped, or otherwise returned to the free state.

## Consequences

Async call sites must keep request objects, buffers, descriptors, and callbacks alive for the active lifetime. This makes lifetime visible and allows intrusive queues without allocation, but it puts more responsibility on callers and higher-level wrappers. Reusing a request from callbacks is allowed only after the request state permits it.

## Confirmation

A change preserves this decision when async operations still store their active state in caller-owned `AsyncRequest` objects, active requests are not heap-allocated by the event loop, tests cover request stop/reuse/lifetime behavior, and documentation continues to state the stable-address requirement.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [Async documentation](../../Documentation/Libraries/Async.md)
- [AsyncRequest](../../Libraries/Async/Async.h)
