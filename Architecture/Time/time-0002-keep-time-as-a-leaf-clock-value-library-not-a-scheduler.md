# TIME-0002 - Keep Time as a Leaf Clock/Value Library, Not a Scheduler

Status: Accepted
Date: 2026-07-04

## Context

Many libraries need time values, deadlines, or timeout durations, but making Time the owner of scheduling, sleeping, timers, or event-loop behavior would turn it into a dependency magnet. Higher-level libraries can represent timing with primitives or their own backend-specific timer mechanisms.

## Decision

Time remains a dependency-free leaf library for clock reads, time values, parsing, literals, arithmetic, and conversions. Scheduling, sleeping, event-loop timers, async timeout management, and task coordination belong in Threading, Async, Await, or consuming libraries.

## Consequences

Time stays small and independently consumable. Consumers that need scheduling must compose with another library instead of expecting Time to own execution. Some libraries may use primitive timeout values to avoid depending on Time, which is acceptable when it protects their adoption boundary.

## Confirmation

A change preserves this decision when Time documentation reports no dependencies, Time public APIs do not introduce thread/event-loop/scheduler ownership, consumers can still avoid depending on Time for simple timeout storage, and scheduling features are added to the appropriate execution library instead.

## Related

- [Time documentation](../../Documentation/Libraries/Time.md)
- [Time public API](../../Libraries/Time/Time.h)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0015 - Prefer maturing existing libraries over expanding scope](../Global/sc-0015-prefer-maturing-existing-libraries-over-expanding-scope.md)
