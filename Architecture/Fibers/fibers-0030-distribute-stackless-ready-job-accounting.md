# FIBERS-0030 - Distribute Stackless Ready-Job Accounting

Status: Accepted
Date: 2026-07-26

## Context

Transactional external publication reduced one million sustained jobs to 3,907 global queue claims, and persistent
workers removed thread startup from repeated waves. Diagnostics then showed balanced execution and few steals, but
every local pop and successful steal still decremented one scheduler-wide ready counter. Independent-record payloads
therefore continued to contend on a shared cache line even after active-job completion accounting was distributed.

`readyJobCount()` and `hasReadyJobs()` must remain bounded and lifetime-safe. In particular, worker parking must never
observe a false zero while ready jobs move from the external queue into a worker deque.

## Decision

Distribute ready-job accounting while a multi-worker `FiberJobWorkerPool` is running, symmetrically with active-job
accounting from FIBERS-0027.

- Jobs in the external queue remain charged to the scheduler-wide ready counter.
- A global claim adds the transferred local jobs to the claiming worker's cache-line-isolated ready counter before it
  subtracts the full claimed batch from the scheduler counter. The immediately executing job is not added to the
  worker counter.
- A worker-local spawn increments its owner's ready shard before publishing the deque bottom.
- Owner pops decrement their own shard. A successful steal decrements the victim's shard because accounting ownership
  follows the deque where the ready job was published.
- `readyJobCount()` sums the scheduler counter and the bounded worker shards. A concurrent transfer can briefly
  overcount but cannot report a false zero. Stable observations remain exact.
- One-worker pools and manually driven schedulers retain the original single ready counter.

All storage remains embedded in caller-owned worker records. The change adds no allocation and no unbounded scan.

## Consequences

Parallel local execution no longer writes one scheduler-wide ready cache line for every tiny job. Steals can still
write another worker's shard, but measured steals are a small fraction of sustained jobs and the contention remains
distributed. Worker idle checks perform a bounded worker scan, which is off the successful execution hot path.

The Draft public `FiberJobWorker` layout changes, so binary consumers must recompile.

## Confirmation

Tests must hold all workers inside claimed callbacks and verify that the stable ready count exactly excludes running
jobs, then cancel and drain the local backlog to zero. Deque release must assert every ready shard is empty. Sustained
benchmarks must report claim, steal, and worker-distribution diagnostics so throughput changes can be attributed.

## Related

- [FIBERS-0027 - Distribute stackless active-job accounting](fibers-0027-distribute-stackless-active-job-accounting.md)
- [FIBERS-0029 - Publish contiguous job batches transactionally](fibers-0029-publish-contiguous-job-batches-transactionally.md)
