# FIBERS-0015 - Retain Task Group Records Until Explicit Reset

Status: Accepted
Date: 2026-07-15

## Context

`FiberTaskGroup` keeps task pointers so callers can inspect failures through `countErrors()` and `collectErrors()` after
the group counter reaches zero. A `FiberTaskPool`, however, normally makes a completed task slot available immediately
after the fiber returns to its worker root context. Reusing that slot while it is still linked into a task group can
overwrite the result observed by the group or link the same task into the group twice.

The group cannot copy an unbounded error history without caller-provided storage, and hidden allocation is not an
option. Error inspection therefore needs an explicit bounded lifetime tied to the task records already supplied by the
caller or task class.

## Decision

`FiberTaskGroup` retains every task record in one submitted wave after completion. A retained task is completed but is
not reusable by a direct spawn, fixed `FiberTaskPool`, or `FiberTaskClass` until `FiberTaskGroup::reset()` succeeds.

`reset()` requires the group's pending count to be zero. It unlinks every retained task, releases any class-backed task
slot, and makes fixed-pool task slots observable as available. The group destructor performs the same reset and
release-asserts if the group still has pending work.

Group linkage is established during scheduler task initialization before the first ready publication. This prevents a
worker from completing and recycling a task before the spawning thread has linked it to the group. Stack-class slots
remain independent task execution storage and may be released at normal root-context completion; only the task record
is retained for error identity and result inspection.

A group represents one retained wave at a time. New group spawns fail until the previous completed wave has been reset.
This keeps result lifetime explicit and avoids silently discarding error history.

## Consequences

Error collection remains allocation-free and preserves stable `FiberTask*` identity. Pool capacity used by a group is
held until reset, so callers must collect errors and reset promptly before expecting those task slots to become
available. Stacks can still be reused independently once execution is safely back on a worker root context.

The API gains one explicit lifecycle operation instead of introducing hidden error storage, a fixed undocumented error
limit, or task handles with shared ownership.

## Confirmation

A change preserves this decision when group linkage happens before first publication, completed group task records are
not recycled before reset, reset fails while work is pending, class-backed task slots are released exactly once, fixed
pool capacity becomes available after reset, and error collection observes the original task and result.

## Related

- [FIBERS-0003 - Keep task and stack lifetimes caller-owned and memory-stable](fibers-0003-keep-task-and-stack-lifetimes-caller-owned-and-memory-stable.md)
- [FIBERS-0009 - Keep FiberTaskPool as the ergonomic facade over task and stack classes](fibers-0009-keep-fibertaskpool-as-the-ergonomic-facade-over-task-and-stack-classes.md)
- [Fibers architecture](fibers-architecture.md)
