# FIBERS-0003 - Keep Task And Stack Lifetimes Caller-Owned And Memory-Stable

Status: Accepted
Date: 2026-07-07

## Context

Fiber context switching stores execution state in task objects and stack memory. Moving, reusing, or freeing that memory
while a fiber is running, waiting, or completing can corrupt execution. The runtime also needs bounded task and stack
pools to support millions of jobs over time without one task object per submitted job.

## Decision

`FiberTask` objects and fiber stack storage remain caller-owned or explicitly class-owned, non-movable while active, and
stable until the scheduler has safely completed the fiber. Pooled tasks return to availability only after completion has
switched back to the worker root context. `FiberTaskPool`, `FiberTaskClass`, and `FiberStackClass` provide bounded reuse
without hiding ownership.

## Consequences

The runtime can safely recycle task and stack slots under pressure, and callers can reason about memory budgets. APIs
must expose capacity and waiting primitives instead of pretending spawn always succeeds. The lifecycle is stricter than
heap-owned task handles, but it matches the rest of SC.

## Confirmation

A change preserves this decision when active tasks and stacks are never moved, completion does not make pooled storage
available before returning to the worker root context, and task/stack pool capacity pressure remains explicit through
queries, waits, or `Result` errors for real failures.

## Related

- [ASYNC-0001 - Keep AsyncRequest objects caller-owned and memory-stable](../Async/async-0001-keep-asyncrequest-objects-caller-owned-and-memory-stable.md)
- [SC-0006 - Use explicit result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
- [Fibers architecture](fibers-architecture.md)
