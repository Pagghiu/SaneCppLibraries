# FIBERS-0001 - Keep Fibers Independent From Async Await And Threading

Status: Accepted
Date: 2026-07-07

## Context

`Fibers` needs OS threads, synchronization, scheduling, and eventually I/O integration. Reusing existing libraries can
save implementation work, but Sane C++ Libraries deliberately keeps dependency graphs small so individual libraries stay
independently consumable.

## Decision

Keep core `Fibers` independent from `Async`, `Await`, and `Threading`. Small OS-thread and synchronization pieces may be
implemented locally inside `Fibers` when that avoids a dependency and the duplicated code remains contained. Async I/O
integration belongs in `FibersAsync`.

## Consequences

`Fibers` remains usable as a CPU micro-tasking runtime without pulling in async I/O or coroutine machinery. Some native
thread and synchronization code is duplicated, but that duplication is intentional while it stays small. Any future
dependency must be justified by substantial shared surface and recorded in a new ADR.

## Confirmation

A change preserves this decision when `Libraries/Fibers` does not include public headers from `Async`, `Await`, or
`Threading`, and when I/O-specific types remain outside the core `Fibers` public API.

## Related

- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0010 - Treat Common as source-sharing not a library](../Global/sc-0010-treat-common-as-source-sharing-not-a-library.md)
- [Fibers architecture](fibers-architecture.md)
