# FIBERS-0036 - Coalesce All Local Job Wakes Behind Backlog

Status: Accepted
Date: 2026-08-11

## Context

FIBERS-0035 suppressed redundant operating-system signals for worker-local multi-job batches appended behind visible
local backlog, but retained unconditional wake-one behavior for worker-local single jobs. Continuation-heavy workloads
publish many single jobs and therefore continued to serialize through the condition-variable mutex while thieves
already had visible work available.

An optimized macOS ARM64 Time Profiler capture of the pinned depth-six Skynet workload attributed about 40% of
eight-worker samples to `FiberJobWorkerPool::wakeOneWorker`, with about 34% in the slow pthread mutex path. The local
single-job path was the dominant caller. This is the same publication state and parking race already analyzed by
FIBERS-0035; the number of jobs in one publication does not change the wake contract.

## Decision

Every worker-local job publication distinguishes whether its deque was empty immediately before publication:

- publishing into an empty local deque advances the generation and requests one peer wake;
- appending behind existing local backlog advances the generation without requesting an operating-system signal.

This rule applies to both single-job and multi-job publication. External single-job publication retains wake-one
behavior, external batch publication retains wake-all behavior, and terminal completion and shutdown retain their
existing wake behavior. The implementation adds no allocation, dependency, public layout, or scheduler-global lock.

## Consequences

Continuation-heavy fan-out avoids repeated condition-variable serialization while preserving the
prepare-recheck-park protocol. In controlled macOS ARM64 Release measurements of the checksum-validated million-node
Skynet workload, the six-worker median moved from about 49.4 ms to 19.4 ms. A post-change eight-worker profile reduced
`wakeOneWorker` from about 40% to below 1% inclusive sampled time and the slow mutex lock path from about 34% to below
1%. These are diagnostic results, not portable publication claims.

One local publisher does not request additional parallelism for every job appended behind existing work. Active peers
can steal that backlog, an empty deque still wakes one peer, and every publication still advances the generation to
close the publication-versus-park race.

## Confirmation

The decision remains valid when empty local deques wake one peer, non-empty local deques advance the generation,
single-job and batch fan-out complete while peers transition through parking, external publication and shutdown retain
their existing wake behavior, and the full Debug/Release and supported-platform suites pass. Performance confirmation
must compare identical benchmark revisions, worker counts, allocation policies, timing boundaries, warm-ups, and
measured rounds.

## Related

- [FIBERS-0024 - Reject Unnecessary Wake Locking Atomically](fibers-0024-reject-unnecessary-wake-locking-atomically.md)
- [FIBERS-0035 - Coalesce Job Batch Wakes Behind Local Backlog](fibers-0035-coalesce-job-batch-wakes-behind-local-backlog.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
