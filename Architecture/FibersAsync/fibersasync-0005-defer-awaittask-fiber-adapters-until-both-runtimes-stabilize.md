# FIBERSASYNC-0005 - Defer AwaitTask Fiber Adapters Until Both Runtimes Stabilize

Status: Accepted
Date: 2026-07-07

## Context

`Await` and `Fibers` both sit above `Async`, but they use different execution models: C++20 coroutine frames for
`Await`, and stackful tasks for `Fibers`. An adapter between `AwaitTask` and fiber tasks could be useful, but premature
integration could lock in accidental ownership, cancellation, scheduling, or allocator semantics.

## Decision

Do not add direct `AwaitTask` to fiber-task adapters yet. Revisit the adapter only after both runtimes have stable
lifecycle, cancellation, scheduling, and real-use bridge requirements.

## Consequences

`Await` and `Fibers` can continue maturing independently while sharing the lower-level `Async` model. Users must choose
one high-level execution model for now or bridge manually at explicit boundaries. A future adapter can be designed from
real usage instead of speculation.

## Confirmation

A change preserves this decision when `FibersAsync` does not expose `AwaitTask` APIs, `Await` does not depend on
`Fibers`, and any future bridge proposal documents ownership, cancellation, allocator, and scheduler interaction before
implementation.

## Related

- [Await architecture](../Await/await-architecture.md)
- [Fibers architecture](../Fibers/fibers-architecture.md)
- [FibersAsync architecture](fibersasync-architecture.md)
