# FIBERS-0010 - Use Worker-Owned Scheduling With Bounded Injection

Status: Accepted
Date: 2026-07-10

## Context

The first worker-deque implementation established bounded work stealing and task migration, but normal tiny-fiber
execution still repeatedly takes one scheduler-global lock for lifecycle transitions, ready publication, and worker
coordination. This creates substantial lock contention and spin retries as worker counts rise.

Removing isolated lock scopes has already produced regressions because it changes both stealing opportunities and the
ordering of readiness publication. The next step needs one explicit ownership model that also covers cancellation,
external producers, parking, and shutdown.

## Decision

Move the scheduler toward worker-owned hot-path state with a separate, bounded control/injection path.

- A worker exclusively owns local deque push/pop and its normal ready-to-running, running-to-ready, and
  running-to-completing transitions. Thieves only claim the opposite deque end through the deque's atomic steal
  operation.
- An external thread or a non-owner worker must not push directly into another worker's deque. Such work uses a
  scheduler-owned bounded MPSC injection queue in the first implementation. Its storage is explicit through
  `FiberAllocator`/worker-pool setup; per-worker injection queues are a later measured optimization, not an initial
  requirement.
- If a bounded injection queue is full, new work creation reports its existing capacity error. Publication of an
  already-existing ready fiber must remain allocation-free and cannot expose normal user-visible backpressure; it may
  use the intrusive global ready spill under the scheduler control path.
- Task state has one logical owner at a time. Owner-only transitions use that ownership directly; a non-owner operation
  such as cancellation or a wait completion must claim a legal transition with an atomic state gate before publishing
  the task. A task can be published ready exactly once per suspension.
- Waiting queues belong to their synchronization primitive. Counter/event completion and cancellation coordinate through
  that primitive's lock plus the task state gate, rather than through the scheduler-global hot-path lock.
- Active-fiber enumeration becomes worker-owned control-plane state. Normal completion removes itself in O(1); rare
  cancellation-by-token and cancel-all operations coordinate worker registries without making each completion scan or
  serialize on one global active list.
- Worker termination and parking use atomic activity/ready-work signals. A worker follows prepare, recheck, then park;
  ready publication occurring during that sequence cancels the park or wakes one worker. Shutdown and cancellation
  broadcasts still wake all workers.

The target state transition rules are:

| From | To | Owner / synchronization |
|:--|:--|:--|
| `Ready` | `Running` | deque owner after local pop, or thief after successful steal; task state gate prevents double claim |
| `Running` | `Ready` | current worker publishes locally; non-owner publication enters bounded injection or intrusive spill |
| `Running` | `Waiting` | current worker links under the waited primitive's lock after recording suspension intent |
| `Waiting` | `Ready` | primitive completion or cancellation claims the task once, unlinks it, then publishes it |
| `Running` | `Completing` | current worker records final `Result` and reduces active work |
| `Completing` | `Completed` | worker root context performs post-switch pool/class release and availability notification |

## Consequences

The common CPU path no longer needs one scheduler lock per fiber state change, while externally initiated work remains
bounded and explicit. Existing fibers retain no-allocation yield/wake behavior. Cancellation remains cooperative and
does not preempt running stacks, but its race rules become explicit enough to test independently.

The first implementation intentionally favors one bounded scheduler injection queue over a more complex hierarchy. It
may still contend under heavy external submission, but it keeps ownership auditable and gives benchmarks a concrete
baseline before adding per-worker injection or batch stealing.

This decision requires more per-task and per-worker metadata, careful atomic memory-order documentation, and expanded
race testing. It does not authorize hidden allocation, unbounded queues, or an implicit global scheduler.

## Alternatives Considered

- Keep the global scheduler lock and optimize individual scopes: rejected because prior partial changes regressed
  throughput and retained the central scalability limit.
- Let any producer push to any worker deque: rejected because it violates the deque's single-producer invariant and
  makes task publication races substantially harder to audit.
- Use an unbounded externally submitted queue: rejected because it violates caller-selected capacity and no-allocation
  rules.
- Start with per-worker injection queues: deferred until a bounded scheduler-level injection queue is measured; it adds
  routing, sizing, and fairness policy without first proving it is necessary.

## Confirmation

A change preserves this decision when normal owner execution does not take the scheduler-global hot-path lock, external
and cross-owner work never mutates another worker's deque directly, every ready publication has one owner, injection
capacity is explicit, parking cannot lose work, and cancellation/primitive races prove no task is lost, double-resumed,
or completed twice.

## Related

- [FIBERS-0004 - Use bounded worker deques with intrusive global spill for work stealing](fibers-0004-use-bounded-worker-deques-with-intrusive-global-spill-for-work-stealing.md)
- [FIBERS-0006 - Keep cancellation cooperative and wake-based](fibers-0006-keep-cancellation-cooperative-and-wake-based.md)
- [FIBERS-0007 - Model spawn backpressure as explicit capacity waiting](fibers-0007-model-spawn-backpressure-as-explicit-capacity-waiting.md)
- [FIBERS-0008 - Use stack-local waiter nodes for cooperative waits](fibers-0008-use-stack-local-waiter-nodes-for-cooperative-waits.md)
- [Fibers active runtime roadmap](../../Documentation/Plans/FibersPlan.md)
- [Fibers architecture](fibers-architecture.md)
