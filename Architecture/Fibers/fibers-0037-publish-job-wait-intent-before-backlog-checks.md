# FIBERS-0037 - Publish Job Wait Intent Before Backlog Checks

Status: Accepted
Date: 2026-08-11

## Context

FIBERS-0035 suppresses an operating-system signal when a worker-local job batch is appended behind visible local
backlog, but still advances the shared wake generation for every such publication. A profile of the depth-six Skynet
workload attributed about 43% of all samples to that contended atomic generation increment.

Simply omitting the increment when no worker is parked is unsafe. A worker can finish its final ready-work check while
publication occurs and then sleep without observing either the work or a generation change. The runtime needs a
publication-versus-wait handshake that does not place every active publisher on one contended atomic write.

## Decision

`FiberJobWorkerPool` tracks workers preparing to wait separately from workers blocked in the platform condition wait.
A worker publishes prepare-to-wait intent, executes a sequential fence, and only then captures the wake generation and
performs its final scheduler-ready check. It retracts intent whether the check finds work or the condition wait returns.

After publishing a non-empty local batch behind existing backlog, the publisher executes a sequential fence and reads
the prepare-to-wait count. It advances the generation without an operating-system signal only when at least one worker
has published intent. The paired store-load barriers prevent publication and intent from both remaining unobserved:
either the publisher sees intent and changes generation, or the later worker check sees the published work.

Publishing a local batch into an empty deque still wakes one worker. Local single-job publication still wakes one
worker. External publication, terminal completion, cancellation, and shutdown keep their existing wake behavior.
The public `parkedWorkerCount()` continues to report workers actually blocked in the platform wait, not preparing
workers.

This supersedes FIBERS-0035 only where it requires every non-empty local batch publication to advance generation. It
narrows the every-publication rules in FIBERS-0022 and FIBERS-0024 only for that same stackless backlog path.

## Consequences

Active local batch publishers no longer contend on a shared generation write when no worker can race into a wait.
Workers pay one pool-local intent update and one fence when they exhaust ready work. The publisher and worker barriers
are required correctness operations and must not be removed based only on current processor behavior.

The intent count uses fixed storage in `FiberJobWorkerPool`; the change adds no allocation, dependency, lock, or opaque
wake-event growth. Existing stronger publication semantics remain available where backlog alone cannot prove adequate
parallelism.

A quiet macOS ARM64 Release comparison used one warm-up and five measured runs per series. The six-worker one-million-
job Skynet median-of-series decreased from 15.853 ms to 6.516 ms. This is diagnostic optimization evidence, not a
portable performance claim.

## Confirmation

A change preserves this decision when workers publish intent before their final ready check, backlog publishers order
work before inspecting intent, both sides retain the store-load barrier, intent is retracted on every wait path, and
pool join observes zero remaining intent. A repeated immediate-parking test must publish one local batch behind a
guaranteed earlier backlog and complete without stranded jobs. Local empty-deque batches, local single jobs, external
publication, cancellation, and shutdown must retain their stronger wakes. Debug and Release tests must pass on the
supported platform families.

## Related

- [FIBERS-0022 - Coalesce Redundant Worker Wake Signals](fibers-0022-coalesce-redundant-worker-wake-signals.md)
- [FIBERS-0024 - Reject Unnecessary Wake Locking Atomically](fibers-0024-reject-unnecessary-wake-locking-atomically.md)
- [FIBERS-0035 - Coalesce Job Batch Wakes Behind Local Backlog](fibers-0035-coalesce-job-batch-wakes-behind-local-backlog.md)
- [FIBERS-0036 - Reject Coalescing All Local Job Wakes Behind Backlog](fibers-0036-coalesce-all-local-job-wakes-behind-backlog.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
