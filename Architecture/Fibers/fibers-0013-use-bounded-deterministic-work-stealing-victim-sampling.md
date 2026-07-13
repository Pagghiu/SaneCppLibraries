# FIBERS-0013 - Use Bounded Deterministic Work-Stealing Victim Sampling

Status: Accepted
Date: 2026-07-10

## Context

An idle worker currently looks for stealable work by inspecting every worker and choosing the largest backlog. This
cost grows linearly with worker count and occurs exactly when the worker is already unable to make local progress.
Always finding the global maximum gives little benefit for bounded deques when any ready victim is sufficient to
restore work sharing.

Random victim selection would avoid a full scan but makes tests and benchmark comparison less reproducible. A fixed
first-victim policy can repeatedly favor the same workers and harms fairness under persistent imbalance.

## Decision

Each `FiberWorker` stores a caller-owned round-robin steal cursor, initialized to the worker immediately after it in a
configured group. One steal attempt examines at most two eligible victims from the supplied worker group, skipping the
thief itself. It chooses the larger observed ready backlog from that bounded sample, then advances the cursor past the
inspected positions. The worker attempts the existing atomic single-task steal only from the selected victim.

`FiberWorkerDiagnostics` exposes `stealVictimProbes` alongside attempted, successful, and failed steals. This makes the
bounded policy observable in focused tests and benchmarks. The policy does not change deque ownership, task migration,
or allocation behavior; it only chooses which existing deque to query.

## Consequences

Steal selection has a bounded small cost independent of normal worker-pool size, while round-robin cursor advancement
eventually samples every configured worker. A temporarily unsuitable sample may fail even when a different worker has
work; later idle attempts advance to other candidates. This trade-off is intentional and must be judged with forced
imbalance and balanced CPU benchmarks.

Batch stealing, randomized comparison, topology-aware victim groups, and adaptive sampling remain separate measured
optimizations. They must not be added merely to compensate for the scheduler-global hot-path lock targeted by
FIBERS-0010.

## Alternatives Considered

- Scan every worker and choose the largest backlog: rejected because its idle-path cost grows with worker count.
- Choose one fixed victim: rejected because it can produce persistent unfairness.
- Randomly choose victims: deferred because deterministic sampling is simpler to test and establishes a reproducible
  baseline first.
- Steal batches immediately: deferred until single-task sampling is measured; batch ownership and locality policy need
  separate validation.

## Confirmation

A change preserves this decision when one steal attempt probes no more than two eligible victim queues, does not mutate
another worker's deque except through the existing atomic steal operation, advances deterministic per-worker sampling,
and retains diagnostics for probes and outcomes.

## Related

- [FIBERS-0004 - Use bounded worker deques with intrusive global spill for work stealing](fibers-0004-use-bounded-worker-deques-with-intrusive-global-spill-for-work-stealing.md)
- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [Fibers architecture](fibers-architecture.md)
