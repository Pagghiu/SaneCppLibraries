# FIBERS-0002 - Use Explicit FiberAllocator Storage For Scalable Runtime Memory

Status: Accepted
Date: 2026-07-07

## Context

To run large numbers of fiber jobs over time, `Fibers` needs storage for worker deques, task classes, stack classes, and
future scheduler structures. Hidden heap allocation would make capacity, failure, and latency behavior invisible.
Depending on another library's allocator would also create an unwanted dependency.

## Decision

`Fibers` owns a separate `FiberAllocator` modeled after the explicit allocator strategy used by `Await`, but implemented
inside `Fibers`. The allocator supports caller-provided fixed storage, explicit virtual reservation, explicit malloc
mode, and caller-provided polymorphic allocation. Allocation diagnostics and close-time validation are part of the
contract.

## Consequences

Scalable runtime storage can grow from caller-selected budgets without hidden allocation. Tests and benchmarks can use
explicit malloc mode, while production code can select fixed or virtual storage. The cost is an additional allocator
surface in `Fibers`, but it keeps dependency and memory policy visible.

## Confirmation

A change preserves this decision when new scalable runtime memory is allocated through an explicit `FiberAllocator`,
caller-provided spans, or documented caller-owned storage; allocation failures are observable; and close validation can
detect leaked allocator-owned blocks.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [AWAIT-0002 - Require explicit coroutine frame allocation](../Await/await-0002-require-explicit-coroutine-frame-allocation.md)
- [Fibers architecture](fibers-architecture.md)
