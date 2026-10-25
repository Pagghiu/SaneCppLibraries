# ASYNCEXECUTION-0002 - Owner Dispatch and Scheduler Affinity

Status: Accepted
Date: 2026-10-19

## Context

CPU continuations and native I/O need explicit execution resources without making the owner loop wait for running worker code. Borrowed contexts and the optional owning runtime must share the same composition and cancellation contracts.

## Decision

AsyncExecutionContext borrows an AsyncEventLoop and owns an intrusive command dispatcher. Cross-thread start and stop publish owner commands; stop registration is removed before terminal delivery. Native timer/socket cancellation joins Async quiescence. Worker-backed and filesystem operations retain their normal acknowledgement callback when cancellation would otherwise block the owner or discard irreversible results. CPU work cannot be preempted. Resource-specific acknowledgement rules remain explicit.

Loop scheduling completes successfully on the owner. Pool scheduling completes on a worker, and downstream then runs there. Use ThreadPool afterRelease to notify after the task node is released; keep owner progress independent of CPU continuation duration. Embedded task storage and notification-tail ownership are specified in ASYNCEXECUTION-0011. Resources survive producer threads and notification return. Advertise conservative weakly_parallel progress and value-only completion affinity; submission-error/stopped affinity is not promised.

Prefer standard on/starts_on/continues_on and ordinary composition. on restores the scheduler supplied by the caller environment, which may differ from the owner loop. Retain onPool as an SC convenience: on a valid running context its body runs on the runtime pool and completion returns through Async to that context owner, including worker-started use. context.work is the corresponding borrowed primitive. These conveniences express owner binding; they do not replace the pool scheduler or standard on.

The optional AsyncExecution facade owns a loop, context and pool. Whole-file and TCP conveniences own their buffers/paths/connections. Allocation failures use Result. Core Async ownership/allocation rules and adapter independence remain unchanged. Callers join graphs and producers before explicit close or destruction; observable shutdown and structured graph ownership have separate decisions.

## Confirmation

Use gated CPU work to prove timer/TCP owner-loop progress, queued and running stop behavior, error identity and terminal storage destruction. Distinguish worker scheduler continuation, standard on restoration and owner-loop onPool completion. Exercise cross-thread submission, failed queueing, native quiescence and pool notification-tail joins. Record exact platform/configuration evidence separately.
