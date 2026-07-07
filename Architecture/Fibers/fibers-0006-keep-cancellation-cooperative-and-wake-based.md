# FIBERS-0006 - Keep Cancellation Cooperative And Wake-Based

Status: Accepted
Date: 2026-07-07

## Context

Fiber stacks contain ordinary C++ execution state. Preemptively unwinding or aborting a running fiber would violate
destructor, ownership, and no-exception assumptions. At the same time, waiting fibers must be able to observe
cancellation promptly enough for task groups, pools, and synchronization primitives to shut down predictably.

## Decision

Cancellation in `Fibers` is cooperative. Requesting cancellation marks cancellation state and wakes affected waiting
fibers, but it does not preempt a running fiber, unwind its stack, or force an exception. Fiber code observes
cancellation through explicit tokens, wait results, and normal `Result` propagation.

## Consequences

The scheduler can remain allocation-free and compatible with exceptions disabled. Cancellation is predictable at
suspension and wait points, but CPU-bound fiber code must check cancellation explicitly if it needs faster response.
Synchronization primitives and pools must remove canceled waiters safely before returning.

## Confirmation

A change preserves this decision when cancellation APIs do not asynchronously destroy fiber stacks, waits are woken on
cancellation, canceled waiters are unlinked exactly once, and examples propagate cancellation through `Result` rather
than exceptions.

## Related

- [SC-0006 - Use explicit result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
- [FIBERS-0008 - Use stack-local waiter nodes for cooperative waits](fibers-0008-use-stack-local-waiter-nodes-for-cooperative-waits.md)
- [Fibers architecture](fibers-architecture.md)
