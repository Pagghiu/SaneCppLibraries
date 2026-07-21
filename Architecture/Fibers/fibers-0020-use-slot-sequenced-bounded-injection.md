# FIBERS-0020 - Use Slot-Sequenced Bounded Injection

Status: Accepted
Date: 2026-07-21

## Context

FIBERS-0018 removed scheduler-global coordination from ordinary external spawn, but its dedicated injection lock still
serializes context initialization, queue publication, worker claims, and active-registry transfer. A synchronized
multi-producer benchmark measured thousands of injection contentions and millions of spin retries for 8,192 tiny jobs.

A two-phase lock experiment was rejected because separate capacity-reservation and publication transactions increased
lock acquisitions from about 24.6K to 32.8K and generally reduced throughput. The queue must remove producer/consumer
serialization rather than split it.

## Decision

Use one caller-funded bounded queue with a monotonically sequenced slot for each configured injection-capacity unit.

- Producers reserve tail positions with compare-exchange and publish a slot by release-storing its next sequence.
- Workers reserve head positions with compare-exchange, acquire the published sequence, and return the slot to producers
  with a sequence advanced by the queue capacity.
- A monotonic `tail - head` capacity check preserves exact one-slot queue behavior, which the sequence value alone cannot
  distinguish.
- Each slot stores one task pointer and one sequence word. `FiberInjectionSlotStorageSize` exposes the fixed caller-funded
  storage cost; no metadata allocation is hidden.
- Task context initialization occurs after slot reservation and outside injection control. Failed initialization
  publishes a null tombstone; producers and consumers reclaim tombstones without changing ready counts.
- A task is atomically marked as publishing so concurrent duplicate spawn attempts cannot initialize it twice.
- A successfully initialized task joins the pre-claim active registry before its slot becomes visible. Cancellation
  linearizes at that registry/publication boundary. Token cancellation requested during initialization is still observed.
- Injection control remains only for the pre-claim active registry and its cancellation transfer to worker ownership.
- Outstanding publications keep an already-running worker pool from declaring scheduler quiescence.

This supersedes FIBERS-0018's lock-protected queue publication and claim mechanics. Its cancellation registry,
counter/group fallback, bounded-capacity, and no-allocation contracts remain in force.

## Consequences

Multiple producers and consumers no longer serialize on queue indices or pointer publication. Each configured slot now
uses two machine words instead of one, and fixed allocator budgets must include that explicit cost.

The queue can temporarily block later claims behind an earlier producer that reserved but has not published its slot.
This preserves FIFO publication order and bounded storage without spinning inside a worker critical section.

Public Draft scheduler and diagnostics layouts change. Binary consumers must recompile; no compatibility shim is
provided for a Draft library.

## Confirmation

This decision remains valid when saturation, capacity-one wraparound, failed initialization, concurrent producers,
duplicate spawn, cancellation, shutdown, and immediate reuse tests pass; ready counts exclude tombstones; all publication
counts return to zero before release; and the multi-producer benchmark materially reduces injection coordination cost or
improves throughput.

## Related

- [FIBERS-0018 - Separate injection control from scheduler coordination](fibers-0018-separate-injection-control-from-scheduler-coordination.md)
- [FIBERS-0019 - Use backlog-aware injection claim batches](fibers-0019-use-backlog-aware-injection-claim-batches.md)
- [Fibers active runtime roadmap](../../Documentation/Plans/FibersPlan.md)
