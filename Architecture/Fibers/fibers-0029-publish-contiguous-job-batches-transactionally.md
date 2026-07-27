# FIBERS-0029 - Publish Contiguous Job Batches Transactionally

Status: Accepted
Date: 2026-07-26

## Context

Persistent `FiberJob` waves remove thread startup, but publishing thousands of records through scalar external
`spawn()` makes the producer and workers contend on the bounded injection lock and emits redundant wake notifications.
Publishing every child from one worker avoids that lock but concentrates the wave in one owner deque and causes a steal
storm. Neither path represents balanced bounded external submission.

The runtime cannot allocate entry arrays, hide overflow, or leave half a batch active after an error. Pointer batches
would also need separate uniqueness validation or caller storage.

## Decision

Add `FiberJobScheduler::spawn(Span<FiberJob>, Procedure[, FiberCancellationToken])` for contiguous stable records.

- The scheduler validates the open state, non-empty span, fixed procedure, bounded queue capacity, and every job before
  changing any record.
- Contiguous span elements are inherently distinct, so transactional validation needs no allocation or quadratic
  duplicate scan.
- Publication initializes and enqueues every record under one queue lock, advances ready and active accounting once,
  then wakes all workers after the queue becomes visible.
- Every job receives the same copied fixed-size `Function` and optional cancellation token. Per-job identity remains
  available through `FiberJobContext::job()`.
- Scalar `spawn()` retains worker-local publication and remains preferable for individual or recursive child jobs.

## Consequences

External producers can submit balanced bounded waves with one synchronization acquisition and one wake phase. Callers
that need different procedures or non-contiguous pooled records continue using scalar publication until evidence
justifies a separate explicit batch-entry API.

Public caller-sized layouts are unchanged by this overload, and no new dependency or hidden storage is introduced.

## Confirmation

A change preserves this decision when capacity or record validation failure leaves every input unchanged, batch
cancellation completes every record normally with failed `Result`, one batch wake makes progress from an idle
persistent pool, scalar owner-local publication is unchanged, sustained benchmarks report the publication boundary,
and single-file builds remain dependency-free.

## Related

- FIBERS-0026
- FIBERS-0028
