# FIBERS-0027 - Distribute Stackless Active Job Accounting

Status: Accepted
Date: 2026-07-26

## Context

The first parallel `FiberJob` worker pool decremented one scheduler-wide active counter after every completed callback.
Repeated one-million-job measurements showed a large one-to-many-worker throughput collapse after queue claims had
already been batched and the scheduler's hot fields had been isolated onto separate cache lines. The exact active count
was the remaining cache line written by every completing worker.

Termination, shutdown, and caller-visible diagnostics still need bounded exact accounting. Replacing the counter with
an approximate flag or delaying arbitrary completion updates could report a false zero and release caller-owned job,
worker, deque, or allocator storage too early.

## Decision

Use distributed active-job ownership only while a multi-worker `FiberJobWorkerPool` is running.

- Jobs in the external queue remain charged to the scheduler-wide active counter.
- A worker transfers an entire claimed batch from the scheduler counter to its cache-line-isolated worker counter with
  one aggregate add and subtract. The worker count is incremented before the scheduler count is decremented, so a
  concurrent snapshot may temporarily overcount but can never observe a false zero.
- Accounting ownership is recorded in each stable `FiberJob` before a claimed local deque range is release-published.
  A stolen job keeps its original accounting owner and decrements that owner's counter on completion.
- Jobs spawned by a pooled worker are charged directly to that worker before local publication. External and manually
  driven jobs retain scheduler-wide accounting.
- Stable `activeJobCount()` sums the scheduler count and the bounded pool worker counters exactly. Worker termination
  performs that bounded scan only after a worker finds no runnable job, not after every completion.
- The one-worker path keeps the original scheduler-wide counter because it has no inter-worker cache-line contention
  and does not benefit from ownership transfer.

All counters remain in caller-owned public records. The implementation adds no allocation and no queue growth.

## Consequences

Multi-worker callbacks no longer write one globally contended completion cache line. Batch claims add two aggregate
atomic operations and completion normally writes only one ownership shard. Stealing may make a thief write another
worker's shard, but contention is distributed across bounded claim owners rather than forced through one scheduler
location.

`activeJobCount()` is exact when ownership is stable and conservative during the short transfer window. This is enough
for control paths because overcounting can delay termination briefly, while false zero would violate lifetime safety.
The Draft public `FiberJob` and `FiberJobWorker` layouts change, so binary consumers must recompile.

## Confirmation

Tests must hold every worker inside a running callback after batched claims and observe the original total active count,
then prove cancellation and join reduce it to zero. Startup rollback and deque release must assert every worker shard is
zero before caller storage becomes reusable. Release benchmarks must compare one, two, four, eight, and available
workers with one warm-up and at least five measured samples.

## Related

- [FIBERS-0025 - Prototype stackless jobs with separate bounded scheduling](fibers-0025-prototype-stackless-jobs-with-separate-bounded-scheduling.md)
- [FIBERS-0026 - Use separate worker-owned scheduling for stackless jobs](fibers-0026-use-separate-worker-owned-scheduling-for-stackless-jobs.md)
