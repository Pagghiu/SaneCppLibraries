# FIBERS-0036 - Reject Coalescing All Local Job Wakes Behind Backlog

Status: Rejected
Date: 2026-08-11

## Context

FIBERS-0035 suppresses redundant operating-system signals for worker-local multi-job batches appended behind visible
local backlog, but retains wake-one behavior for worker-local single jobs. Continuation-heavy workloads publish many
single jobs and can therefore serialize through the condition-variable mutex while parked workers exist.

An optimized macOS ARM64 Time Profiler capture of the pinned depth-six Skynet workload attributed about 40% of
eight-worker samples to `FiberJobWorkerPool::wakeOneWorker`, with about 34% in the slow pthread mutex path. Applying the
batch backlog rule to single jobs reduced the six-worker median from about 49.4 ms to 19.4 ms, which made the
generalization initially attractive.

Cross-platform validation exposed a semantic counterexample. A running parent can publish single jobs whose procedures
must begin concurrently before any can finish. Waking only one peer for the first local job leaves later peers parked,
because existing backlog suppresses their signals, and the active publisher and first child then wait forever. The
scheduler cannot infer from deque depth whether an active job is available to drain that backlog.

## Decision

Reject backlog-based signal suppression for the general worker-local single-job API. Every local single-job
publication retains wake-one semantics, including pending-signal coalescing and the no-parked-worker fast path from
FIBERS-0022 and FIBERS-0024.

Callers that publish a group of jobs with shared parallelism intent may use transactional batch publication, including
a one-record batch when existing work already supplies sufficient parallelism. Batch publication retains the
empty-deque/backlog rule from FIBERS-0035.

## Consequences

Arbitrary single jobs preserve progressive parallelism even when currently active procedures are cooperatively
blocked. Continuation-heavy workloads must express stronger publication intent through the batch API or accept the
general wake-one cost; the scheduler does not guess from transient worker or deque state.

The Skynet benchmark publishes aggregation continuations as one-record batches because child fan-out already supplies
parallelism. Controlled macOS ARM64 Release samples retain the improvement without changing the general runtime
contract, but remain diagnostic rather than portable publication claims.

## Confirmation

The decision remains valid when a parent can sequentially publish enough single jobs to start every worker before any
child completes, batch publication retains its backlog-aware wake behavior, repeated Debug/Release worker-pool tests do
not hang, and supported-platform suites pass. Reviews must reject scheduler heuristics based only on local deque depth
or an estimated active-worker count because active procedures may be blocked.

## Related

- [FIBERS-0022 - Coalesce Redundant Worker Wake Signals](fibers-0022-coalesce-redundant-worker-wake-signals.md)
- [FIBERS-0024 - Reject Unnecessary Wake Locking Atomically](fibers-0024-reject-unnecessary-wake-locking-atomically.md)
- [FIBERS-0035 - Coalesce Job Batch Wakes Behind Local Backlog](fibers-0035-coalesce-job-batch-wakes-behind-local-backlog.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
