# FIBERSASYNC-0002 - Keep AsyncEventLoop Owner-Thread Affine With Bounded Command Posting

Status: Accepted
Date: 2026-07-07

## Context

`AsyncEventLoop` interaction is thread-affine, while fiber tasks can run and resume on worker threads. Starting or
stopping async requests directly from arbitrary worker threads would violate the lower-level event-loop contract.
Unbounded cross-thread queues would violate SC allocation rules.

## Decision

`FiberAsyncIO` keeps one owner thread for the wrapped `AsyncEventLoop`. Operations requested from other threads are
posted through bounded caller-provided command storage and executed by the owner thread. Worker fibers suspend
cooperatively until the owner-thread async request completes, fails, or is canceled.

## Consequences

`FibersAsync` can support fiber migration across worker threads while preserving `Async`'s thread-affinity assumptions.
Callers must size command storage for cross-thread starts/stops, and command queue exhaustion is explicit backpressure
rather than hidden allocation.

## Confirmation

A change preserves this decision when `FiberAsyncIO` can identify the owner thread, cross-thread starts/stops go through
bounded command posting, command queue capacity is visible through `Result` failures for real exhaustion, and tests cover
worker-fiber I/O submission from non-owner threads.

## Related

- [ASYNC-0002 - Split event-loop running into submit poll and dispatch phases](../Async/async-0002-split-event-loop-running-into-submit-poll-and-dispatch-phases.md)
- [FIBERS-0004 - Use bounded worker deques with intrusive global spill for work stealing](../Fibers/fibers-0004-use-bounded-worker-deques-with-intrusive-global-spill-for-work-stealing.md)
- [FibersAsync architecture](fibersasync-architecture.md)
