# FIBERS-0022 - Coalesce Redundant Worker Wake Signals

Status: Accepted
Date: 2026-07-22

## Context

Every normal ready publication advances the worker-pool wake generation and signals one condition-variable waiter.
Advancing the generation is required even when no worker is parked: a worker may have captured the prior generation
and completed its ready-work recheck immediately before publication. Omitting that generation change can lose the
wake and park a worker while work is ready.

Bursty publication creates a different inefficiency. Several notifications can arrive before workers already selected
by earlier signals reacquire the wake mutex. Signaling again after every parked worker has a signal pending cannot add
parallelism, but still enters the operating-system condition-variable path.

## Decision

The wake event tracks the number of individual signals that are pending for currently parked workers. Normal ready
publication always advances the generation, preserving the prepare-recheck-park protocol. It emits an individual OS
signal only when fewer signals are pending than workers are parked. A worker consumes one pending signal after waking.

Wake-all remains reserved for explicit pool stop and terminal scheduler transitions that require every worker to
recheck shutdown state. A broadcast marks every currently parked worker as pending before waking all of them.

`FiberWorkerPoolWakeDiagnostics` exposes generation-changing `wakeNotifications` and coalesced individual
`wakeSignals`. Diagnostics reset after each validated pool start attempt and remain readable after join. They allocate
no memory and use the wake event's existing mutex rather than adding scheduler-global coordination.

## Consequences

The runtime preserves wake-one semantics while bounding redundant signal fan-out by the number of parked workers.
Notification generation still takes the wake mutex, so this decision does not remove wake-event serialization. The
diagnostic notification count includes wake-all operations; `wakeSignals` intentionally excludes broadcasts.

FIBERS-0024 later removes that notification-mutex requirement while preserving this decision's generation and pending
signal contracts.

FIBERS-0037 later narrows the every-publication generation rule for stackless local batches appended behind existing
backlog. That path uses a separate prepare-to-wait handshake and paired barriers instead.

A quiet macOS ARM64 Release comparison used one warm-up and five measured runs, followed by a complete confirmation
set. In the confirmation set, individual signals fell by 85-99.7% for high-worker tiny-work cases. Sustained
eight-worker throughput improved 8.9%, useful-payload four/eight-worker throughput remained within 1.4% of the prior
baseline, and four-worker external-producer throughput improved 14.3%.

## Alternatives Considered

- Skip notifications when no worker is currently parked: rejected because it loses the generation change needed to
  close the publication-versus-park race.
- Signal for every publication: correct but rejected because already-pending signals cannot wake additional workers.
- Wake all for ordinary ready publication: rejected because it creates a thundering herd and weakens wake-one policy.
- Track an unbounded notification queue: rejected because the count is bounded by caller-provided workers and no
  allocation is needed.

## Confirmation

A change preserves this decision when every ready publication still changes the generation, pending signals never
exceed parked workers, a waking worker consumes its pending signal, normal publication cannot lose work, diagnostics
reset across pool reuse, and shutdown or terminal completion still wakes every parked worker.

FIBERS-0037 defines the only accepted exception to the first condition.

## Related

- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [FIBERS-0014 - Use bounded worker idle spinning](fibers-0014-use-bounded-worker-idle-spinning.md)
- [FIBERS-0024 - Reject unnecessary wake locking atomically](fibers-0024-reject-unnecessary-wake-locking-atomically.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
