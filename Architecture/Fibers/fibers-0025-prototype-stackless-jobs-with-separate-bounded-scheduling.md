# FIBERS-0025 - Prototype Stackless Jobs With Separate Bounded Scheduling

Status: Accepted
Date: 2026-07-22

## Context

`FiberTask` supports suspension through ordinary nested calls by assigning every active task a stack. That semantic and
memory cost should not be imposed on tiny CPU work that always runs to completion. Adding a tagged work item to the
existing fiber deques before measuring a stackless implementation would also put a new branch and larger ownership
surface into the proven stackful scheduler.

The first job slice needs to validate naming, result lifetime, cancellation, recursive submission, bounded capacity,
and baseline overhead. It does not yet need to decide whether a production parallel job runtime shares fiber worker
threads or owns a separate concrete worker pool.

## Decision

Add `FiberJob` as a stable caller-owned run-to-completion record and `FiberJobScheduler` as its first concrete bounded
scheduler. The scheduler receives caller-owned `Span<FiberJob*>` ready storage, never grows it, and initially runs only
when its owner calls `runOne()` or `run()` on one thread.

`FiberJob::Procedure` receives a restricted `FiberJobContext` rather than `FiberScheduler`. The context exposes the
current job, its `FiberJobScheduler`, and cooperative cancellation checks. It does not expose yield, fiber wait
primitives, or `FibersAsync`. A running job may submit another stable job when queue capacity is available.

Job errors are stored in the completed `FiberJob`; scheduler-driving methods return errors only for scheduler
operations. Pending cancellation skips the procedure and stores a cancellation error. A running job is never
preempted, but may observe cancellation through its context. Completed result storage remains in the caller-owned job
until that object is explicitly spawned again.

`FiberJobPool` is the first reusable fixed-storage facade. It acquires records from a caller-provided `Span<FiberJob>`
in O(1), but does not recycle a completed record automatically. The caller inspects its retained result and explicitly
calls `release()`. Failed publication returns the acquired record immediately, so queue backpressure cannot silently
consume pool capacity. Allocator-backed job classes remain a later extension of the same retention contract.

The initial scheduler is deliberately not thread-safe. Parallel workers, job groups, reusable job pools/classes, and
help-while-full backpressure require a subsequent ADR informed by this API and benchmark. This is a topology boundary,
not permission to funnel jobs through stackful task queues.

## Consequences

The prototype requires one pointer of ready capacity per simultaneously queued job and no stack memory. Capacity
failure is immediate and deterministic. Nested submission naturally consumes the slot freed when the current job was
claimed, but a callback cannot recursively drive the scheduler while it is already running.

Pool capacity counts retained records, including completed results awaiting explicit release. This intentionally keeps
result lifetime visible and prevents a later spawn from overwriting a record that the caller still holds.

An initial macOS ARM64 Release run of ten million jobs in reusable 8,192-job batches measured five samples from 57.2
million to 78.2 million jobs per second, with a median of 74.0 million. This measures the manually driven stackless
spawn/run/completion path only; it is not evidence for parallel scaling or a competitor comparison.

`FiberJob` and `FiberJobScheduler` are public Draft layouts. Binary consumers must recompile, as already required by the
Draft ABI policy. No dependency, hidden allocation, exception, RTTI, STL type, or system header is added.

## Alternatives Considered

- Reuse `FiberTask` with an empty stack: rejected because context setup and suspension state remain on the hot path.
- Immediately add jobs to fiber worker deques: deferred because it would choose shared topology before measuring the
  standalone stackless cost and would branch the stackful claim path.
- Build a second OS-thread pool immediately: deferred until fixed-storage job semantics and one-worker overhead are
  validated.
- Return job procedure failures from `run()`: rejected because one failed job is work outcome, not scheduler failure;
  the caller-owned job already retains its plain `Result`.

## Confirmation

A change preserves this decision when every active job has stable caller ownership, ready capacity is explicit,
procedures cannot suspend through the job API, nested submission cannot grow storage, cancellation remains
cooperative, completion retains plain `Result`, pooled results are not reused before explicit release, and the
stackful scheduler hot path is unchanged.

## Related

- [FIBERS-0003 - Keep task and stack lifetimes caller-owned and memory-stable](fibers-0003-keep-task-and-stack-lifetimes-caller-owned-and-memory-stable.md)
- [FIBERS-0007 - Model spawn backpressure as explicit capacity waiting](fibers-0007-model-spawn-backpressure-as-explicit-capacity-waiting.md)
- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
