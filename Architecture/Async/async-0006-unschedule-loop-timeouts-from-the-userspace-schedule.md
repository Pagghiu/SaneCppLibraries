# ASYNC-0006 - Unschedule Loop Timeouts From The Userspace Schedule

Status: Accepted
Date: 2026-08-13

## Context

Caller-owned components sometimes embed an `AsyncLoopTimeout` and must reclaim that storage during synchronous
teardown without dispatching unrelated event-loop callbacks. The ordinary `AsyncRequest::stop` contract completes
asynchronously because most active requests may still be referenced by a native backend, a copied completion batch, or
a worker thread.

Loop timeouts are different. Async keeps each timeout in a userspace schedule and native backends only track a shared
event-loop deadline. No backend retains the address of an individual `AsyncLoopTimeout` after it becomes active.

## Decision

`AsyncLoopTimeout::unschedule` synchronously removes an unsequenced timeout from the userspace schedule, suppresses its
callback, and returns the request to the free state. The operation is idempotent for an already-free timeout. The caller
must serialize it with event-loop submission, polling, and callback dispatch.

Native shared deadline maintenance may happen during a later poll. In particular, io_uring keeps a shared timeout in a
removing state until its cancellation completion is consumed and does not reuse that completion identity while removal
is pending.

This operation is not generalized to `AsyncRequest`. Other active request types can remain kernel-owned after
cancellation is requested and therefore cannot provide the same synchronous storage-release guarantee.

## Consequences

Owners of embedded loop timers can perform non-reentrant synchronous teardown without pumping the event loop. Sequenced
timers continue to use sequence cancellation because removing one request directly would bypass sequence bookkeeping.
Callers that cannot serialize with the event loop must use the ordinary asynchronous stop contract.

## Confirmation

A change preserves this decision when queued and active unsequenced timers become free without callback dispatch,
already-free timers remain idempotent, sequenced and transitional timers reject unscheduling without mutation, and
forced io_uring tests cover removal and replacement after a shared native deadline has been armed.

## Related

- [AsyncLoopTimeout](../../Libraries/Async/Async.h)
- [ASYNC-0001 - Keep AsyncRequest objects caller-owned and memory-stable](async-0001-keep-asyncrequest-objects-caller-owned-and-memory-stable.md)
- [Async documentation](../../Documentation/Libraries/Async.md)
