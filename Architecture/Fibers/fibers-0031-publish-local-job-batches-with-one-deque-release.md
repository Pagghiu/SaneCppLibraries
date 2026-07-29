# FIBERS-0031 - Publish Local Job Batches With One Deque Release

Status: Accepted
Date: 2026-07-26

## Context

FIBERS-0029 made external contiguous `FiberJob` batches transactional, but a running job still published recursive
fan-out one scalar child at a time. Ten-child Skynet nodes consequently repeated ready and active accounting updates,
deque-bottom release stores, and worker wake decisions even when all child records were contiguous and the owner deque
had enough caller-funded capacity.

## Decision

Extend the existing `spawn(Span<FiberJob>, Procedure, token)` contract to use the current job worker's local deque when
the complete batch fits.

- Validate every record before changing any job state, counter, or deque slot.
- Initialize all records and write all stable pointers before publishing the new deque bottom once.
- Add ready and active accounting once for the complete batch, using the worker shards selected at pool startup.
- Wake one pooled peer only after the whole range is visible so it can steal from the opposite deque end. External
  batch publication still wakes every parked worker because it must bootstrap parallel progress without an active
  owner; owner-local publication favors gradual recruitment and avoids broadcasting at every recursive fan-out node.
- If the complete batch does not fit locally, retain the FIBERS-0029 bounded external-queue transaction and its exact
  rollback behavior. Never split one API call across local and external storage.
- One-worker pools and manually driven workers continue using scheduler-wide counts; only queue placement changes.

The operation remains allocation-free and bounded by caller-provided job, deque, and external queue storage.

## Consequences

Recursive fan-out can amortize accounting, publication, and wake overhead without adding continuation allocation or a
new job type. The all-or-nothing placement rule keeps backpressure deterministic and avoids partial publication state.
One-peer wakeup avoids repeated condition-variable broadcasts for fine-grained recursive jobs. It guarantees progress,
not immediate use of every parked worker; subsequent publications recruit more peers as the workload unfolds.

Callers that relied on worker-originated batch jobs entering the external queue may observe different execution order.
The library is Draft, and owner-local publication is already the established scalar-spawn policy.

## Confirmation

Manually driven tests must publish a batch from a running owner, observe the complete batch on that worker's deque,
steal from the opposite end, and drain exactly once. A pooled regression must also prove that a local batch wakes a
parked peer when idle spinning is disabled. External validation, capacity failure, cancellation, and reuse tests remain
mandatory. Recursive fan-out benchmarks must compare the local transaction against scalar publication.

## Related

- [FIBERS-0026 - Use separate worker-owned scheduling for stackless jobs](fibers-0026-use-separate-worker-owned-scheduling-for-stackless-jobs.md)
- [FIBERS-0029 - Publish contiguous job batches transactionally](fibers-0029-publish-contiguous-job-batches-transactionally.md)
- [FIBERS-0030 - Distribute stackless ready-job accounting](fibers-0030-distribute-stackless-ready-job-accounting.md)
