# FIBERS-0008 - Use Stack-Local Waiter Nodes For Cooperative Waits

Status: Accepted
Date: 2026-07-07

## Context

Synchronization primitives, task pools, task classes, and stack classes need to suspend fibers until another fiber or
worker makes progress. Allocating waiter records for these waits would violate the no-hidden-allocation model and create
extra failure modes in core scheduling paths.

## Decision

Cooperative wait operations use stack-local waiter nodes or intrusive storage owned by the waiting object. The waiter is
linked before suspension, unlinked on notification or cancellation, and must not outlive the suspended wait call.

## Consequences

Wait operations remain allocation-free and naturally bounded by the number of suspended fibers. Implementations must be
careful about cancellation and notification races because the waiter storage is only valid while the waiting call is
active.

## Confirmation

A change preserves this decision when new waits do not allocate waiter records, canceled waits remove their waiter before
returning, notification wakes at most the intended waiters, and tests cover cancel/notify races for each new primitive.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [FIBERS-0006 - Keep cancellation cooperative and wake-based](fibers-0006-keep-cancellation-cooperative-and-wake-based.md)
- [Fibers architecture](fibers-architecture.md)
