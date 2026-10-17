# ASYNCEXECUTION-0004 - Preserve Loop Progress During Pool Continuations

Status: Accepted
Date: 2026-10-17

## Context

The prototype queued pool-ticket cleanup on the owner loop before notifying its receiver. That cleanup called waitForTask while the worker could be executing the entire downstream CPU continuation. A bounded regression reproduced the resulting stall: neither the timer nor TCP branch could progress until an independent failsafe released the CPU gate.

## Decision

Use the ThreadPool afterRelease notification defined by THREADING-0004. The scheduling task has a trivial body. Once the pool has released the task node, the copied worker notification captures the operation/delivery pointers, destroys and frees the independent ticket, and drops its context outstanding count. It then invokes the receiver-dependent delivery function on that same worker. There is no owner-loop cleanup command and no owner-loop wait for running CPU work.

No ticket/context storage is accessed after receiver delivery begins. The receiver may destroy its operation. The pool remains alive until the copied notification returns; pool destruction joins it. Borrowed execution resources must still outlive producer threads and active graphs, while the owning runtime retains them through run. Stop before delivery produces set_stopped; ordinary synchronous then work cannot be preempted. A later stop-aware I/O or scheduler observes a stop requested during that computation.

This supersedes the pool-ticket reclamation order in ASYNCEXECUTION-0002 and extends the non-template implementation boundary in ASYNCEXECUTION-0003. It does not change native Async request cancellation or add adapter-to-adapter dependencies.

## Consequences

Loop progress no longer depends on CPU continuation duration. Pool scheduling still allocates a ticket, and execution resources must outlive their producers. Full-pool joins cover notification tails; cancellation of running CPU functions remains cooperative.

## Confirmation

The deterministic timer/TCP progress test must fail on the old implementation and pass after this change, using a failsafe independent of the owner loop. Test queued stop, stop during CPU work, completion/stop races and operation destruction from both value/stopped delivery. Revalidate dedicated tests/demos across platforms, default regression suites and amalgamation.
