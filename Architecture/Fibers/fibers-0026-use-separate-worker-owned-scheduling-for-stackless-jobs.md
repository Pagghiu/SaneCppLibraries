# FIBERS-0026 - Use Separate Worker-Owned Scheduling For Stackless Jobs

Status: Accepted
Date: 2026-07-25

## Context

FIBERS-0025 proves that stable, stackless `FiberJob` records can execute tens of millions of run-to-completion callbacks
per second on one manually driven scheduler. The next step must add parallel execution and work stealing without putting
a job/task tag, a larger queue element, or a new branch into the established stackful `FiberTask` hot path.

Bounded capacity has two distinct causes. A ready queue can be full even though another worker can execute queued work
and create room. A `FiberJobPool` can also have no available record because every record is running, queued, or retained
for result inspection. Executing more retained jobs does not release those records, so blindly waiting for pool capacity
can deadlock. The runtime needs to state which pressure it can relieve rather than describing both as "full".

The shared Skynet benchmark also requires recursive completion dependencies. A run-to-completion callback cannot wait
for children without either becoming a stackful task or publishing a continuation. Parallel scheduling alone therefore
does not make the current flat `FiberJobGroup` an honest Skynet backend.

## Decision

Add a separate concrete stackless worker runtime around `FiberJobScheduler`. It owns no hidden memory and receives
caller-provided worker/thread storage plus queue storage obtained explicitly from `FiberAllocator`.

- Each job worker exclusively pushes and pops one bounded local deque. Thieves claim the opposite end using the same
  single-owner work-stealing principle as stackful workers, but with job-specific records and diagnostics.
- External producers publish stable job pointers through one bounded scheduler-level injection queue. They never push
  directly into a worker-owned deque. External queue exhaustion remains an immediate `Result` error.
- A running job publishes a child to its local deque. If that deque and bounded fallback publication are full, the
  worker executes one available job and retries publication. Helping is iterative and stack-bounded; it must not recurse
  through the C++ call stack.
- Helping guarantees relief only for ready-queue pressure. If `FiberJobPool` acquisition fails, the spawn reports that
  capacity error after an optional bounded help attempt. It must not block indefinitely because explicit result
  retention can make record capacity unavailable until caller-controlled `release()` or group `reset()`.
- Queue capacity and record capacity remain separately observable in diagnostics. Neither may grow implicitly.
- A queued job may migrate through stealing. A running job stays on its current OS thread until its callback returns.
  There is no suspension, preemption, fiber primitive wait, or `FibersAsync` operation in `FiberJobContext`.
- Pending cancellation claims a job before execution and stores cancellation in its normal `Result`. Running
  cancellation is cooperative and observable through the context. Completion publishes the final result exactly once
  before notifying a group or future continuation.
- Worker-pool `join()` drains accepted work. Explicit stop requests cancellation and wakes all workers before draining;
  no job record, queue slot, worker, or allocator storage may be reused before join completes.

The first implementation uses separate job queues and worker threads. Sharing physical worker threads with
`FiberTask`, using a tagged unified work-item deque, and runtime-selectable topology remain measured alternatives. A
unified topology is not permitted unless equivalent workloads demonstrate a material benefit that justifies changing
the stackful hot path.

`FiberJobGroup` remains the retained-result completion model. Recursive fork/join benchmarks require a later bounded
continuation or auto-recycling counter model. That model must preserve plain `Result`, explicit storage, and stable
failure aggregation; it must not emulate suspension inside a run-to-completion callback.

Pool retention and group membership bookkeeping are serialized before parallel workers are introduced. A grouped job
is linked to its group before scheduler publication, so a worker can never report completion to an uninitialized owner.
If publication fails after record acquisition, group membership and pool retention both roll back before `spawn()`
returns. This does not make the manually driven `FiberJobScheduler` itself safe for overlapping calls.

## Consequences

Parallel job workers can scale CPU callbacks independently from stackful fibers and can be sized without reserving
stacks. Queue saturation from nested fan-out can make progress through useful work rather than sleeping or asking the
application to drive `runNoWait()`. Pool sizing remains part of the API contract because retained result identity and
unbounded recursive fan-out cannot both be guaranteed with finite storage.

The separate runtime duplicates a contained amount of worker lifecycle, parking, and diagnostics code. That is
preferred to coupling the two execution models or adding a new library dependency. Shared internal source fragments may
be extracted only when they preserve independent public libraries and do not force a unified scheduling abstraction.

Public job, scheduler, worker, and diagnostics layouts remain Draft ABI. Binary consumers must recompile after layout
changes; no compatibility shim is required.

## Alternatives Considered

- Add jobs to `FiberWorker` deques: rejected initially because every stackful claim would need a type distinction and
  the proven task scheduler would inherit job-specific completion and retention rules.
- Use one global locked job queue: rejected as the production target because it cannot provide competitive balanced
  scaling; it may be retained only as a diagnostic baseline.
- Block a worker until any capacity becomes available: rejected because all workers can hold running records while
  retained records cannot be recycled, creating a bounded-capacity deadlock.
- Grow queues or allocate overflow nodes: rejected because capacity and allocation must remain caller-controlled.
- Let a job suspend while waiting for children: rejected because that is `FiberTask` semantics and requires a stack.

## Confirmation

A change preserves this decision when stackful task queues and claim paths remain unchanged, every job queue is bounded
and caller-funded, external publication reports saturation, local queue pressure helps useful work without recursive
stack growth, record exhaustion cannot wait forever, queued jobs execute at most once, cancellation and shutdown drain
without lost work, and diagnostics distinguish queue occupancy from retained record occupancy.

Before declaring the topology complete, focused tests must cover local saturation, injection saturation, wraparound,
stealing, cancellation-before-claim, cancellation while running, stop/drain, startup rollback, immediate storage reuse,
and the all-workers-fan-out record-exhaustion case. Release benchmarks must cover one, two, four, eight, and available
workers with balanced, forced-steal, recursive fan-out, and tiny sustained workloads.

## Related

- [FIBERS-0004 - Use bounded worker deques with intrusive global spill for work stealing](fibers-0004-use-bounded-worker-deques-with-intrusive-global-spill-for-work-stealing.md)
- [FIBERS-0007 - Model spawn backpressure as explicit capacity waiting](fibers-0007-model-spawn-backpressure-as-explicit-capacity-waiting.md)
- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [FIBERS-0020 - Use slot-sequenced bounded injection](fibers-0020-use-slot-sequenced-bounded-injection.md)
- [FIBERS-0025 - Prototype stackless jobs with separate bounded scheduling](fibers-0025-prototype-stackless-jobs-with-separate-bounded-scheduling.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
