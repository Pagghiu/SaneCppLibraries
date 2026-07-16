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
| `Running` | `Suspending` | current worker records the wait action, primitive, interruptibility, and preferred worker before switching to its root context |
| `Suspending` | `Waiting` | root context links the task under the waited primitive's lock only when the primitive remains incomplete and cancellation did not win |
| `Suspending` | `Ready` | root context observes a completion or cancellation that raced with suspension and publishes the task only after the old stack is inactive |
| `Waiting` | `Ready` | primitive completion or cancellation claims the task once, unlinks it, then publishes it |
| `Running` | `Completing` | current worker records final `Result` and switches to its root context |
| `Completing` | `Completed` | worker root context reduces active work, performs post-switch pool/class release, and notifies availability |

`Suspending` is an internal state, not a new public `FiberTaskStatus`. It models the existing two-phase suspension
invariant explicitly: an external completion or cancellation may record that it won, but must never queue a task while
its stack is still executing. The root context is the only code that turns such a recorded outcome into a ready
publication. This is also why cancellation remains cooperative rather than preempting a running stack.

## Control-Plane Signals And Parking

The replacement for the globally locked counters consists of three control-plane signals. They are implementation
fields, not new public API.

- `activeFiberCount` is an atomic count. The creating thread increments it before a task's first ready publication;
  the worker root context decrements it exactly once after final completion has made later resume impossible and before
  the task/stack slot can be recycled. It includes `Ready`, `Running`, `Suspending`, `Waiting`, and `Completing` tasks.
- `readyWorkCount` is an atomic conservative count. A publisher increments it before making a task visible in a local
  deque, injection queue, or intrusive spill. The deque owner or thief decrements it only after successfully claiming
  a visible task. During publication it may temporarily over-count, which is safe: a worker that observes a non-zero
  value must retry work discovery rather than conclude that the scheduler is idle.
- `shutdownRequested` is an atomic flag. Shutdown publication wakes all workers and prevents accepting further work
  according to the public shutdown contract; it does not make active fiber state disappear.

Task initialization happens-before first publication. `activeFiberCount` increments and `readyWorkCount` increments
use release semantics; readers that decide whether to park or terminate use acquire semantics. A successful task-state
gate claim uses acquire-release semantics, with acquire semantics on a failed observation. The existing deque
publication/steal acquire-release protocol protects the task pointer and its initialized fields. These rules make the
activity counters conservative control signals, not an alternative ownership mechanism for task memory.

The worker parking sequence is deliberately prepare, recheck, then park:

1. Explore local work, bounded injection/spill work, and the configured steal attempts.
2. Capture the wake-event generation while its mutex is held, then release that mutex.
3. Acquire-load `shutdownRequested`, `readyWorkCount`, and `activeFiberCount`.
4. Retry discovery when ready work is non-zero. Exit only when shutdown is requested and active work is zero. Otherwise
   wait only while the wake-event generation is unchanged.

Every ready publisher increments `readyWorkCount` before it advances the queue publication point and then increments
the wake generation before signaling. Therefore a worker either observes the ready work during the recheck or sees the
generation change when it tries to wait; no ready publication can be lost between recheck and parking. Normal ready
publication wakes one worker. Shutdown and explicit broadcast operations wake all workers.

## Active Registries And Cancellation

Active-task enumeration is split by ownership instead of using one scheduler-wide active list.

- A task awaiting its first claim from an external producer lives in the bounded injection registry protected by the
  injection control lock.
- Once a worker claims a task, that worker becomes its home worker and the task is linked in that worker's intrusive
  active registry. It remains in that registry while it is ready, running, suspending, or waiting; migration changes
  execution ownership but does not require moving the control-plane registry entry.
- Completion unlinks the task from its home registry in O(1) at the root-context reclamation point. Inactive class and
  pool slots are in no active registry.

Rare cancellation-by-token and cancel-all operations set the cancellation source first, then inspect the injection
registry and worker registries one at a time. The lock order is cancellation source, injection control, worker control,
then waited primitive. Normal primitive completion never acquires a worker-control lock while holding a primitive lock.
This gives cancellation a finite, allocation-free traversal while keeping normal completion and deregistration out of a
scheduler-global lock. A cancellation scan claims a task's state gate before unlinking a waiter or publishing it, so a
counter/event completion racing with cancellation observes an already claimed transition and cannot double-resume the
task.

## Two-Phase Suspension Races

When a running task starts an interruptible wait, it first publishes the `Suspending` intent under the task state gate
and records `suspendAction`, `suspendCounter`, and `suspendInterruptible`. A primitive completion or cancellation
during this interval only records the winning outcome under the same gate. It does not enqueue the task. Once control
has returned to the worker root context, that context either links the still-pending task into the primitive's waiter
queue as `Waiting`, or consumes the recorded outcome and publishes it as `Ready`. A completion/cancellation after the
waiter link follows the normal `Waiting -> Ready` rule. The root context clears suspension metadata only after one of
these paths has become authoritative.

This makes these three races explicit test cases: completion-before-link, cancellation-before-link, and
completion-versus-cancellation-after-link. Each must prove exactly one ready publication and no switch to a still
running stack.

Counter values use atomic transitions independently from their intrusive waiter queue. A decrement known to be
non-final can complete without scheduler coordination. The final `1 -> 0` transition remains serialized with counter
increment and waiter publication so a concurrent increment cannot start a new generation before stale waiters are
woken. Counter-backed task completion publishes `Completed` and leaves its worker active registry before notifying the
counter; consequently a resumed group waiter cannot observe a child still owned by its completing worker root.

A configured deque owner may claim a fixed batch of up to four tasks while it already holds the scheduler control lock.
The first task becomes its immediate `Running` claim; retained tasks stay `Ready`, transfer to stable worker-registry
ownership, and enter the owner's bounded deque in reverse source order so later LIFO owner pops preserve FIFO execution.
The batch never mixes the intrusive spill source with the injection source, never exceeds available deque capacity, and
does not change publication backpressure. The fixed bound prevents one worker from draining an unbounded shared backlog
before peers can steal it.

A running configured pool worker may spawn a counter-free child directly into its own deque when bounded capacity is
already available. Context construction leaves the child inactive; publication atomically increases active work, links
the child into that worker's stable active registry, changes it to `Ready`, and only then exposes the deque slot. This
ordering lets cancellation scans linearize either before the child exists or after it is discoverable. Counter-backed
children, manual workers, and full local deques retain the scheduler-control path and its existing bounded-injection
capacity error. No fallback allocation or implicit queue growth is introduced.

A configured pool worker may also steal directly from another bounded worker deque without the scheduler-control lock.
The successful head-slot compare/exchange gives the thief exclusive ownership of each task. A stolen batch is capped at
one immediately runnable task plus the thief's observed local capacity, so retaining the remainder cannot require an
unlocked global spill. Failed steals still enter the scheduler-control path to inspect the intrusive ready list and
bounded injection queue; manual worker groups retain their coordinated behavior.

Per-worker park-attempt and parked-wakeup diagnostics use layout-preserving atomic counters. Updating observability
therefore does not acquire the scheduler-control lock around the existing wake-generation and OS-wait protocol.

Scheduler-lock diagnostics classify every acquisition as spawn publication, ready/steal fallback, synchronization,
completion fallback, or control-plane work. The categories are observational and sum to the existing acquisition
count; they do not add locks, allocation, or a new scheduling state.

An atomic global-ready count tracks the exact subset of ready tasks held by the intrusive fallback list and bounded
injection queue. After a configured owner misses its local deque and sampled victims, a zero count proves there is no
global queue to inspect under the scheduler-control lock; the worker can retry its bounded victim cursor directly. A
concurrent global publisher updates the count while holding the control lock and wakes a worker, so observing zero may
delay that publication only until the normal wake/retry boundary and cannot lose work.

Counter completion detaches the entire intrusive waiter list before publishing any waiter to a local or global ready
queue. A local publication can be stolen and resumed without the scheduler-control lock, so leaving the counter linked
until after publication would allow a resumed fiber to unwind stack-local counter storage before completion finished
traversing it. Every ready-source claim also proves `Ready -> Running` with compare/exchange rather than overwriting an
unexpected state.

## Consequences

The common CPU path no longer needs one scheduler lock per fiber state change, while externally initiated work remains
bounded and explicit. Existing fibers retain no-allocation yield/wake behavior. Cancellation remains cooperative and
does not preempt running stacks, but its race rules become explicit enough to test independently.

The first implementation intentionally favors one bounded scheduler injection queue over a more complex hierarchy. It
may still contend under heavy external submission, but it keeps ownership auditable and gives benchmarks a concrete
baseline before adding per-worker injection or batch stealing.

This decision requires more per-task and per-worker metadata and expanded race testing. It does not authorize hidden
allocation, unbounded queues, or an implicit global scheduler.

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
