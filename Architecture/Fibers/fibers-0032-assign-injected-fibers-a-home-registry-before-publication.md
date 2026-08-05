# FIBERS-0032 - Assign Injected Fibers A Home Registry Before Publication

Status: Accepted
Date: 2026-08-01

## Context

FIBERS-0020 removed injection-control locking from bounded queue reservation and publication, and FIBERS-0021 removed
scheduler coordination from configured-worker claims. Ordinary externally spawned tasks still entered injection
control twice: the producer linked each task into the pre-claim active registry, then the claiming worker transferred
it into a worker registry. An isolated four-producer profile attributed about 59% of leaf samples to that transfer and
the injection lock. Grouping transfers under one lock was previously rejected because mixed destination ownership
created cancellation lock-order and intrusive-list correctness failures.

The active registry is a control-plane home, not an execution-owner record. A task already remains in one worker's
registry after stealing or migration, so first execution does not need to determine its registry home.

## Decision

Assign every ordinary counter-free, non-group external spawn to a stable worker registry before publishing its bounded
injection slot.

- The reserved monotonic injection position selects a worker registry modulo the configured worker count. This
  distributes producer-side registry locking without hidden storage, random state, or unbounded routing metadata.
- Context initialization completes before the task enters a registry. The producer then increments active work, links
  the task under that worker's registry lock, publishes `Ready`, and only afterward makes the injection slot visible.
- The selected registry remains the task's control-plane home until completion. Claiming, stealing, yielding, waiting,
  and migration do not transfer it.
- Cancellation continues to hold scheduler control while inspecting the global pre-claim registry and each worker
  registry one at a time. It therefore observes an ordinary task either before registry publication or in exactly one
  stable worker registry.
- Coordinated submissions that carry counters or task-group ownership retain the scheduler-controlled path and global
  pre-claim registry. Their existing ordering and release contracts are unchanged.
- Injection capacity, task/stack ownership, wake ordering, and caller-funded storage remain unchanged.

This supersedes FIBERS-0010, FIBERS-0018, FIBERS-0020, and FIBERS-0021 only where they require ordinary injected tasks
to begin in the global pre-claim registry and transfer on first claim.

## Consequences

The common external path no longer acquires injection control for registry publication or claim. Producers can still
contend on worker registry locks, but monotonic distribution bounds that contention across the configured workers.
Cancellation remains allocation-free and finite, and completion retains O(1) removal from a stable registry.

Registry home may differ from the worker that first claims or executes a task. Diagnostics and future policies must not
interpret `activeRegistryWorker` as execution affinity.

The configured worker span and its registry locks must remain alive until worker-pool join completes, matching the
existing worker, deque, task, and stack lifetime contract.

## Confirmation

This decision remains valid when injection saturation, failed publication, duplicate spawn, external cancellation,
mixed transition, shutdown, and immediate-reuse tests pass on supported OS families; no cancellation scan misses or
double-publishes a task; and the isolated external-producer benchmark materially reduces injection-control cost without
regressing throughput.

The first macOS ARM64 Release measurement reduced injection-lock acquisitions from 16,389 to 5 per 8,192-task sample,
with zero measured contention. Ten-sample medians improved from about 0.63M to 1.17M tasks/sec at four workers and from
about 0.48M to 0.78M tasks/sec at eight workers on the same uncontrolled host.

## Related

- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [FIBERS-0018 - Separate injection control from scheduler coordination](fibers-0018-separate-injection-control-from-scheduler-coordination.md)
- [FIBERS-0020 - Use slot-sequenced bounded injection](fibers-0020-use-slot-sequenced-bounded-injection.md)
- [FIBERS-0021 - Claim injection batches without scheduler coordination](fibers-0021-claim-injection-batches-without-scheduler-coordination.md)
- [Fibers active runtime roadmap](../../Documentation/Plans/FibersPlan.md)
