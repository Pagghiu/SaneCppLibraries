# FIBERS-0033 - Claim Contiguous Injection Batches Transactionally

Status: Accepted
Date: 2026-08-01

## Context

After FIBERS-0032 removed common-path registry transfer and injection locking, an isolated four-producer profile
attributed about 25% of leaf samples to `popInjection()`. A configured worker claimed up to sixteen tasks per existing
backlog-aware batch, but each task performed a separate injection-head compare/exchange and separate decrements of the
injection, global-ready, and total-ready counters.

The slot-sequenced ring already exposes a contiguous published prefix. Claiming that prefix does not require changing
the queue's bounded capacity, publication protocol, worker ownership, or maximum batch size.

## Decision

Claim each configured-worker injection batch with one bounded contiguous head reservation.

- A consumer inspects at most the existing claim capacity: sixteen slots with peer workers and four with one worker,
  additionally bounded by its local deque capacity.
- It acquire-loads each consecutive slot sequence until the first unpublished slot, then advances the injection head
  across that visible prefix with one compare/exchange. A failed compare/exchange retries from the winning head.
- The successful consumer exclusively owns every reserved slot. It collects non-null task pointers in FIFO order and
  treats failed-publication tombstones as consumed slots.
- Injection-ready, total-ready, and global-ready counters are each decremented once by the number of real tasks.
- Claimed slot sequences are released to producers only after those counter decrements. Producers therefore cannot
  reuse capacity while the claimed tasks are still represented in queue diagnostics.
- The first real task runs immediately. Remaining tasks enter the claiming worker's deque in reverse insertion order,
  preserving FIFO execution order under owner-side LIFO pops.
- Scalar `popInjection()` remains for coordinated/manual fallback paths.

This supersedes FIBERS-0019 and FIBERS-0021 only where configured workers previously repeated scalar head claims and
counter updates within one bounded injection batch.

## Consequences

The common consumer path performs one head compare/exchange and three counter decrements per batch rather than per task.
The bounded scan adds acquire loads for slots that the old scalar loop would inspect individually. No allocation,
unbounded critical section, new public storage, or new backpressure state is introduced.

Ready counts may conservatively include a claimed batch until its batched decrements complete, as they already could
during scalar claim. Slot capacity is not reusable until the counters are updated, so injection occupancy remains
bounded by the configured capacity.

## Confirmation

This decision remains valid when saturation, wraparound, tombstone, failed-publication reuse, multi-consumer,
cancellation, shutdown, stealing, and mixed-transition tests pass; `injectionClaimBatchPeak` stays within the accepted
bound; and Release throughput or targeted queue cost meets the active roadmap gate.

The first uncontrolled macOS ARM64 ten-sample checkpoint improved four-worker median external throughput from about
1.17M to 1.55M tasks/sec and eight-worker median throughput from about 0.78M to 0.95M tasks/sec.

## Related

- [FIBERS-0019 - Use backlog-aware injection claim batches](fibers-0019-use-backlog-aware-injection-claim-batches.md)
- [FIBERS-0020 - Use slot-sequenced bounded injection](fibers-0020-use-slot-sequenced-bounded-injection.md)
- [FIBERS-0021 - Claim injection batches without scheduler coordination](fibers-0021-claim-injection-batches-without-scheduler-coordination.md)
- [FIBERS-0032 - Assign injected fibers a home registry before publication](fibers-0032-assign-injected-fibers-a-home-registry-before-publication.md)
- [Fibers active runtime roadmap](../../Documentation/Plans/FibersPlan.md)
