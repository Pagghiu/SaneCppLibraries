# FIBERS-0035 - Coalesce Job Batch Wakes Behind Local Backlog

Status: Accepted
Date: 2026-08-09

## Context

`FiberJobScheduler::spawn(Span<FiberJob>)` publishes a worker-local batch with one release of the deque bottom. The
original path then requested one worker wake for every batch, even when that worker already owned visible local
backlog. Dynamic fan-out workloads consequently entered the condition-variable mutex repeatedly while useful work was
already available to thieves.

A symbolized optimized Time Profiler capture of the pinned depth-six Skynet workload attributed about 30% of samples
to `FiberJobWorkerPool::wakeOneWorker` and its pthread mutex path. Four-worker median time was about 50.8 ms. Waking all
parked peers for every local batch was rejected experimentally because it regressed the same workload to about 129 ms.

FIBERS-0024 requires every ready publication to advance the wake generation even when it emits no operating-system
signal. A peer can otherwise capture an old generation during its final ready-work recheck and sleep across a racing
publication.

## Decision

A worker-local multi-job batch distinguishes whether its deque was empty immediately before publication:

- publishing into an empty local deque advances the generation and requests one peer wake;
- appending behind existing local backlog advances the generation without requesting an operating-system signal.

The existing backlog is already represented in distributed ready accounting and therefore prevents a peer's final
ready-work check from entering the condition wait. The generation-only publication closes races where that backlog is
being stolen or drained while the new batch becomes visible.

Single-job local publication retains wake-one semantics. External batch publication retains wake-all semantics.
Terminal completion and shutdown continue to wake all workers. The implementation adds no allocation, dependency,
public layout, or scheduler-global lock.

## Consequences

Dynamic local fan-out avoids repeated condition-variable serialization while retaining the prepare-recheck-park
contract. Initial macOS ARM64 Release samples moved the four-worker depth-six Skynet median from about 50.8 ms to
45.8 ms and the eight-worker median from about 88.4 ms to 59.5 ms. These are diagnostic results, not publication
claims; final evidence must retain raw samples and unchanged allocation/timing disclosures.

A local worker with existing backlog no longer requests additional parallelism for every appended batch. Active peers
can still steal that backlog, and a worker publishing into an empty deque still wakes one peer. Callers remain
responsible for selecting an appropriate worker count; this decision does not add topology or affinity heuristics.

## Alternatives Considered

- Wake one peer for every local batch: correct but rejected because the profile identified its mutex path as the
  dominant sampled cost.
- Wake all parked peers for every local batch: correct but rejected after a large measured thundering-herd regression.
- Skip the wake generation when suppressing the signal: rejected because it violates FIBERS-0024 and can lose the
  publication-versus-park race.
- Add platform-specific affinity or heterogeneous-core detection: rejected because this issue occurs before topology
  policy and callers already choose the bounded worker count.

## Confirmation

The decision remains valid when local empty-deque batches wake one peer, non-empty-deque batches still advance the
generation, publication-versus-park stress does not hang, external batches wake all, shutdown wakes all, and the full
Debug/Release and supported-platform suites pass. Performance confirmation must compare identical Skynet revisions,
worker counts, allocation policy, timing boundaries, warm-ups, and measured rounds.

## Related

- [FIBERS-0022 - Coalesce Redundant Worker Wake Signals](fibers-0022-coalesce-redundant-worker-wake-signals.md)
- [FIBERS-0024 - Reject Unnecessary Wake Locking Atomically](fibers-0024-reject-unnecessary-wake-locking-atomically.md)
- [FIBERS-0029 - Publish Contiguous Job Batches Transactionally](fibers-0029-publish-contiguous-job-batches-transactionally.md)
- [FIBERS-0036 - Reject Coalescing All Local Job Wakes Behind Backlog](fibers-0036-coalesce-all-local-job-wakes-behind-backlog.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
