# FIBERS-0016 - Transfer Fiber Mutex Ownership Before Wake Publication

Status: Accepted
Date: 2026-07-15

## Context

`FiberMutex` suspends contending fibers with stack-local waiter nodes. Unlocking one waiter previously cleared the
current owner, removed that waiter, and only then published its counter wake. Until the selected waiter resumed and
assigned itself as owner, the mutex was still logically locked but had no owner. The releasing fiber could unlock a
second time, or another fiber could pass wrong-owner validation, during that publication window.

A mutex operation made outside a running fiber also has no stable task identity, so it cannot participate in the same
ownership contract.

## Decision

Each mutex waiter records its stable `FiberTask*` while suspended. `unlock()` transfers `owner` to the selected waiter
while holding the primitive lock, before releasing that lock and publishing the wake through `FiberScheduler::done()`.
The resumed waiter verifies that ownership was already transferred rather than creating a temporarily ownerless state.

`lock()` and `unlock()` reject calls made outside a currently running fiber. Event and semaphore signaling remain
cross-thread publication operations because they do not carry mutex ownership.

## Consequences

Mutex handoff remains allocation-free because the owner identity lives in the existing stack-local waiter. The old
owner cannot unlock twice after handing off, wrong-owner checks remain valid throughout wake publication, and a
cancelled selected waiter can release or transfer its already-assigned ownership during cleanup.

The scheduler and waiter task must remain alive under the existing stable-object rules until the wait returns.

## Confirmation

A change preserves this decision when a selected waiter owns the mutex before its wake becomes observable, calls
outside a running fiber fail, a releasing owner cannot unlock twice during handoff, wrong-owner attempts cannot enter
the critical section, and cancellation after selection releases or transfers ownership exactly once.

## Related

- [FIBERS-0006 - Keep cancellation cooperative and wake-based](fibers-0006-keep-cancellation-cooperative-and-wake-based.md)
- [FIBERS-0008 - Use stack-local waiter nodes for cooperative waits](fibers-0008-use-stack-local-waiter-nodes-for-cooperative-waits.md)
- [Fibers architecture](fibers-architecture.md)
