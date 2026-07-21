# FIBERS-0018 - Separate Injection Control From Scheduler Coordination

Status: Superseded in part by FIBERS-0020
Date: 2026-07-20

## Context

FIBERS-0010 assigns externally published tasks to a bounded scheduler injection path, but the initial implementation
protected that path with the scheduler-global lock. A clean external-producer benchmark showed that workers consuming
injected work and the producer publishing it repeatedly serialized on that lock. At higher worker counts, scheduler
spin retries dominated the useful work.

The active-task registry for work awaiting its first worker claim must remain synchronized with publication and
cancellation. Moving only the queue indices to atomics would leave task lifetime and cancellation traversal racy.

## Decision

Give the bounded injection queue and its pre-claim active-task registry a dedicated injection-control lock.

- Counter-free, non-group external spawns initialize, register, and publish under injection control without acquiring
  the scheduler-global lock.
- Counter-backed and grouped spawns retain the scheduler path because they also coordinate intrusive synchronization or
  retention state.
- Workers use injection control only while claiming bounded queue entries or transferring a pre-claim registry entry to
  a stable worker registry. Local execution, stealing, completion, and owner publication remain independent of it.
- Cancellation holds scheduler control while it inspects the injection registry, releases injection control, and then
  inspects worker registries. This preserves a stable transfer boundary and avoids holding injection control while a
  cancellation wake publishes ready work.
- Existing active-fiber wakeups still use the bounded queue when possible and the intrusive spill when it is full. They
  do not expose a capacity error.
- Injection lock acquisitions, contentions, spin retries, and peak retries are observable separately from scheduler-lock
  diagnostics.

This is an intermediate implementation of the scheduler-level MPSC topology selected by FIBERS-0010. Multiple external
producers may call `spawn()` concurrently, but they serialize their short initialization and publication transaction on
injection control. A later slot-sequenced bounded MPSC queue may make queue publication lock-free after cancellation and
pre-claim registry ownership are represented independently. That later change must preserve the contract in this ADR
and requires its own memory-ordering evidence.

## Consequences

The common external publication path no longer competes with worker scheduling for the scheduler-global lock. Storage
remains caller-funded through `FiberAllocator`; capacity, shutdown, cancellation, and immediate-reuse behavior do not
change.

The dedicated lock is still a producer-producer and producer-consumer serialization point. It is intentionally smaller
than the old scheduler critical section and independently measurable, but it is not the final lock-free MPSC design.

The public Draft diagnostics and scheduler layouts grow. Binary consumers must recompile, as already required by the
Draft ABI caveat.

## Alternatives Considered

- Keep using the scheduler-global lock: rejected because measured external submission contention scales poorly and
  obscures injection-specific costs.
- Publish queue pointers atomically while retaining the old active list: rejected because cancellation could race task
  initialization, first claim, and registry transfer.
- Implement slot-sequenced MPSC publication immediately: deferred until the smaller ownership split proves the
  registry, cancellation, and shutdown boundary independently.
- Add one injection queue per worker: deferred until the scheduler-level queue has clean producer and consumer metrics;
  per-worker routing adds a capacity and locality policy that current evidence does not yet require.

## Confirmation

This decision remains valid when ordinary configured external spawns do not increment scheduler `lockSpawn`, injection
capacity errors leave task and stack inputs immediately reusable, cancellation cannot miss a task moving from injection
to worker ownership, existing ready work cannot fail because injection is full, and injection contention is reported
separately from scheduler contention.

## Related

- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [FIBERS-0006 - Keep cancellation cooperative and wake-based](fibers-0006-keep-cancellation-cooperative-and-wake-based.md)
- [Fibers active runtime roadmap](../../Documentation/Plans/FibersPlan.md)
