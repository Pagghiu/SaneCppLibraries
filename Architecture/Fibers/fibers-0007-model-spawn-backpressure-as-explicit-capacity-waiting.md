# FIBERS-0007 - Model Spawn Backpressure As Explicit Capacity Waiting

Status: Accepted
Date: 2026-07-07

## Context

`Fibers` should run very large numbers of jobs over time without requiring one live task object and one live stack per
job. That requires bounded task, stack, allocator, and queue capacity. `Result` should still mean errors, not advisory
pressure, and normal yield/wake publication for existing fibers should not force every call site to handle queue
backpressure.

## Decision

Capacity pressure is explicit at work-creation boundaries. Spawning can fail when caller-provided task, stack, allocator,
or setup capacity is exhausted, and producers can use capacity queries or wait APIs such as
`FiberTaskPool::waitForSpawnCapacity()`. Existing fibers that yield or are woken should publish through reserved
intrusive state, local deques, or global spill without exposing user-visible backpressure.

## Consequences

In-fiber producers can spawn until capacity is exhausted, cooperatively wait, and retry. External producers can block or
drive scheduler work through explicit APIs instead of pretending to be fibers. The API stays bounded and predictable, but
callers must size pools intentionally.

## Confirmation

A change preserves this decision when new spawn-like APIs make capacity exhaustion observable, producer wait APIs do not
allocate hidden storage, yield/wake paths for already-existing fibers do not require advisory out-parameters, and
`Result` remains reserved for errors or cancellation.

## Related

- [FIBERS-0004 - Use bounded worker deques with intrusive global spill for work stealing](fibers-0004-use-bounded-worker-deques-with-intrusive-global-spill-for-work-stealing.md)
- [FIBERS-0009 - Keep TaskPool as the ergonomic facade over TaskClass and StackClass](fibers-0009-keep-taskpool-as-the-ergonomic-facade-over-taskclass-and-stackclass.md)
- [Fibers scale-up plan](../../Documentation/Plans/FibersPlan.md)
