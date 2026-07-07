# FIBERS-0004 - Use Bounded Worker Deques With Intrusive Global Spill For Work Stealing

Status: Accepted
Date: 2026-07-07

## Context

Competitive CPU micro-tasking needs ready fibers to run on any available worker, but the runtime cannot use hidden
allocation or unbounded queues. Yield and wake of already-existing fibers should not expose normal user-visible
backpressure.

## Decision

Use fixed-capacity per-worker deques for the work-stealing hot path. Owners push and pop locally, thieves steal from the
opposite end, and deque storage comes from caller-selected storage or `FiberAllocator`. If a local deque cannot accept an
existing ready fiber, spill that fiber to the scheduler's intrusive global ready list instead of failing the yield or
wake publication.

## Consequences

Ready publication for existing fibers remains allocation-free and does not normally fail. New work creation still obeys
bounded task, stack, allocator, and queue budgets. The global spill path preserves correctness and backpressure policy,
while future optimization can continue reducing scheduler-lock scope around lifecycle and wake handling.

## Confirmation

A change preserves this decision when worker deque capacity is explicit, ready fiber publication never allocates hidden
storage, local deque overflow routes to a bounded/intrusive fallback, and stealing tests prove no task is lost,
double-resumed, or completed twice.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [Fibers scale-up plan](../../Documentation/Plans/FibersPlan.md)
- [Fibers architecture](fibers-architecture.md)
