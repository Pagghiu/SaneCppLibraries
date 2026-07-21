# FIBERS-0021 - Claim Injection Batches Without Scheduler Coordination

Status: Accepted
Date: 2026-07-21

## Context

FIBERS-0020 made injection slot reservation and publication concurrent, but configured workers still entered the
scheduler-global ready lock before claiming each injection batch. The queue indices no longer required that lock, and
active-registry transfer already had separate injection control. Four and eight worker benchmarks continued to report
high scheduler-lock contention and spin retries for tiny externally submitted tasks.

## Decision

A configured deque owner first attempts local work and bounded stealing as before. When injection work is visible, it
claims one bounded batch directly from the slot-sequenced queue without taking the scheduler-global lock.

Each claimed task retains the existing per-task transfer from the pre-claim active registry to worker ownership under
injection control. Retained tasks are published to the owner's deque in reverse insertion order so subsequent LIFO pops
preserve FIFO injection order. The intrusive global spill list remains protected by the scheduler lock and is used as
the fallback when no injection task can be claimed.

`injectionClaimBatchPeak` is updated atomically because several workers can now claim batches concurrently. Empty
manual worker polls do not probe steal victims when the scheduler's total ready count is zero.

## Consequences

Ordinary injection claims no longer serialize on scheduler-global coordination. Cancellation and active-registry
ownership retain their existing linearization and no-allocation behavior.

The injection registry lock becomes more visible after removing the scheduler lock. Grouping several ownership
transfers into one registry transaction was prototyped and rejected: mixed worker-owned injection entries exposed a
lock-order deadlock with cancellation and later active-list corruption. Registry batching requires a separate ownership
design rather than a wider critical section.

Bounded failed-steal backoff and prioritizing global injection work over stealing were also measured and rejected for
this slice because they reduced tiny-task throughput. The deterministic bounded victim policy remains unchanged.

## Confirmation

This decision remains valid when concurrent external spawn, cancellation, shutdown, stealing, mixed transition, and
immediate-reuse tests pass; forced imbalance still steals work; injection claims materially reduce scheduler-lock
coordination; and Release throughput does not regress.

## Related

- [FIBERS-0013 - Use bounded deterministic work-stealing victim sampling](fibers-0013-use-bounded-deterministic-work-stealing-victim-sampling.md)
- [FIBERS-0019 - Use backlog-aware injection claim batches](fibers-0019-use-backlog-aware-injection-claim-batches.md)
- [FIBERS-0020 - Use slot-sequenced bounded injection](fibers-0020-use-slot-sequenced-bounded-injection.md)
- [Fibers active runtime roadmap](../../Documentation/Plans/FibersPlan.md)
