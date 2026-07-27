# FIBERS-0028 - Keep Job Worker Pools Alive Between Waves

Status: Accepted
Date: 2026-07-26

## Context

`FiberJobWorkerPool` originally treated scheduler idleness as terminal. This is simple for one bounded batch, but a
long-lived CPU runtime must recreate every OS thread and worker deque for each wave. It also makes repeated benchmark
samples structurally unlike executors that retain their workers.

Persistent acceptance cannot make queue capacity, shutdown, or the meaning of `join()` implicit. A scheduler may also
receive work from external producers while a control thread is observing idleness.

## Decision

Add opt-in `keepAliveWhenIdle` behavior to `FiberJobWorkerPoolOptions`. The default one-shot lifecycle remains unchanged.

- A persistent pool may start with no active jobs and parks its existing caller-owned workers until publication wakes
  them.
- `waitIdle()` waits until the scheduler has no active jobs. It is a wave boundary, not a barrier against future
  concurrent publication; applications coordinate producers when they require a closed submission boundary.
- Persistent `join()` requires a preceding `requestStop()`. Rejecting an early join avoids silently hanging while the
  pool remains willing to accept work.
- Stop retains the existing cooperative-cancellation contract, wakes all parked workers, drains accepted work, joins
  the threads, and releases every allocator-backed deque.
- Persistent execution uses the existing generation-based wake event and adds no allocation, queue, or hidden storage.

## Consequences

Applications and benchmarks can amortize OS-thread startup across repeated bounded job waves. They must explicitly mark
idle boundaries and stop the pool before final join. Graceful closure against concurrent producers remains an
application-level submission protocol; this API does not add an unbounded accepting queue or silently close the
scheduler.

Public caller-sized layouts remain Draft ABI. Binary consumers must recompile after this change.

## Confirmation

A change preserves this decision when default pools still terminate on idle, persistent pools can start empty and run
multiple waves on the same worker records, wake-up tests use deterministic state rather than sleeps, premature
persistent join fails without tearing down the pool, stop and join release all caller-funded deque storage, and
single-file builds retain no new dependency.

## Related

- FIBERS-0026
- FIBERS-0027
