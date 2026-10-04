# FIBERS-0040 - Opt In To Thread-Pinned Runtime-Owned Scheduling

Status: Accepted
Date: 2026-10-04

## Context

The Fil-C/glibc context prototype works, but its hidden stacks and thread affinity differ from native fibers.
Silently reinterpreting caller-provided stacks or allowing a worker to dequeue an unusable context would violate
ownership and scheduling contracts.

## Decision

Expose `FiberStack::runtimeOwned(size)` as a request with no accessible memory or high-water measurements.
Require `FiberScheduler::enableRuntimeOwnedStacks()` before spawning; it binds the scheduler to that thread.
Expose capability and mode queries. Reject mixed caller/runtime stack modes, worker pools, worker deques,
stealing groups and cross-thread execution before task status or queues change. The owner thread must outlive
its tasks. External cancellation and completion notifications remain allowed.

Use AsyncFiberIO's existing owner-thread event-loop mode with this scheduler. Check scheduler affinity before
draining commands or operating the event loop. Keep cross-thread/worker-pool tests disabled for this backend.
Preserve caller-owned stacks and migration on native compilers; unsupported runtimes return structured errors.

## Consequences

Single-thread asynchronous I/O can suspend on runtime-owned stacks with proper GC visibility. Runtime allocation
is explicit. Guard-page stacks, stack classes/pools, incremental commitment and high-water measurements do not
apply to hidden runtime stacks. Both high-water counters return zero, meaning unavailable, not unused capacity.

## Confirmation

Verify suspend/resume, cancellation, reuse, wrong-thread queue preservation, pool/stealing rejection and the
existing single-thread AsyncFibers sections under glibc Fil-C. Run unsupported-path tests under pizfix and
native scheduler regressions in both configurations.

## Related

- [FIBERS-0039](fibers-0039-prototype-filc-contexts-with-explicit-runtime-owned-stacks.md)
