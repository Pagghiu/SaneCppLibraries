# FIBERS-0019 - Use Backlog-Aware Injection Claim Batches

Status: Accepted
Date: 2026-07-20

## Context

After FIBERS-0018 removed ordinary external publication from the scheduler-global lock, configured workers still took
that lock once for every four injection entries. Tiny external work therefore retained roughly one scheduler-lock claim
per four jobs, and workers contended with one another while a large injection backlog remained.

The four-entry bound in FIBERS-0010 was selected before this cost was independently measurable. It applies well to the
intrusive spill path, where existing-fiber wakeups should remain prompt, but it unnecessarily limits transfer from a
large bounded new-work backlog into worker-local stealable deques.

## Decision

Keep spill claims capped at four entries and allow configured pools with peer workers to claim up to sixteen injection
entries per scheduler-lock acquisition.

- The first claimed task runs immediately. Retained tasks transfer to the claiming worker's active registry and bounded
  deque in reverse insertion order, preserving FIFO execution for the owner's LIFO pops.
- The claim never exceeds observed local deque capacity and never allocates or spills.
- A one-worker pool retains the four-entry batch because no peer can steal a larger transferred backlog and measurements
  did not justify increasing its local burst.
- `injectionClaimBatchPeak` reports the largest observed injection claim independently from injection occupancy and
  spill diagnostics.
- The sixteen-entry limit is an implementation policy, not a public capacity promise. Future changes may tune it only
  with equivalent fairness, ordering, cancellation, and throughput evidence.

This decision supersedes only FIBERS-0010's fixed four-entry limit for bounded injection claims. Its four-entry
intrusive spill behavior and all publication, capacity, registry, and cancellation contracts remain unchanged.

## Consequences

Workers amortize scheduler coordination across a larger ready backlog and expose retained work to existing stealers.
The fixed upper bound prevents one claim from draining an unbounded queue, while peer workers can still obtain work
from the claimant's deque.

A larger transfer can temporarily favor one worker's locality. Forced-steal tests and per-worker diagnostics remain
required to ensure peers participate under useful load.

## Alternatives Considered

- Increase every batch to sixteen: rejected because intrusive wake/spill work has stronger latency and fairness needs.
- Use sixteen entries for a one-worker pool: rejected because the measured gain comes from amortized coordination and
  stealable peer work, neither of which applies without peers.
- Drain the complete injection queue: rejected because it creates an unbounded scheduler critical section and lets one
  worker monopolize ownership transfer.
- Remove scheduler coordination from consumption immediately: rejected after experiments moved contention to injection
  control or created excessive retry and steal storms.

## Confirmation

This decision remains valid when injection claim batches never exceed sixteen or local deque capacity, spill claims
never exceed four, execution order and cancellation ownership remain correct, peers continue stealing forced imbalance,
and multi-worker external-running throughput improves without materially regressing the one-worker path.

## Related

- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [FIBERS-0018 - Separate injection control from scheduler coordination](fibers-0018-separate-injection-control-from-scheduler-coordination.md)
- [FIBERS-0013 - Use bounded deterministic work-stealing victim sampling](fibers-0013-use-bounded-deterministic-work-stealing-victim-sampling.md)
