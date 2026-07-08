# FIBERSASYNC-0001 - Keep FibersAsync As The Bridge From Fibers To Async

Status: Accepted
Date: 2026-07-07

## Context

`Fibers` is the CPU scheduler and `Async` is the low-level I/O runtime. Merging them would make dependencies heavier and
would force CPU-only users to pay for I/O concepts. Duplicating `Async` inside `Fibers` would duplicate too much complex
platform behavior.

## Decision

Keep `FibersAsync` as the explicit bridge library that depends on both `Fibers` and `Async`. Core `Fibers` remains free
of async I/O types, and low-level `Async` remains callback-first. Fiber-friendly I/O helpers live in `FiberAsyncIO`.

## Consequences

CPU-only fiber users can consume `Fibers` without `Async`. I/O users opt into a bridge that exposes both runtime
relationships clearly. The bridge may duplicate a small amount of adapter state, but it avoids duplicating native I/O
backends or hiding an implicit combined runtime.

## Confirmation

A change preserves this decision when `Libraries/Fibers` has no dependency on `Async`, `Libraries/FibersAsync` remains
the location for fiber I/O helpers, and new fiber-aware I/O operations are added to the bridge rather than the core
scheduler.

## Related

- [FIBERS-0001 - Keep Fibers independent from Async Await and Threading](../Fibers/fibers-0001-keep-fibers-independent-from-async-await-and-threading.md)
- [ASYNC-0001 - Keep AsyncRequest objects caller-owned and memory-stable](../Async/async-0001-keep-asyncrequest-objects-caller-owned-and-memory-stable.md)
- [FibersAsync architecture](fibersasync-architecture.md)
