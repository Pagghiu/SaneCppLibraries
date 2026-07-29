# ASYNCFIBERS-0004 - Keep AsyncFiberIO Concrete Until A Second Backend Exists

Status: Accepted
Date: 2026-07-07

## Context

`AsyncFiberIO` currently bridges fibers to `Async`. A runtime-selectable `FiberIO` facade could eventually make sense if
there are multiple real backends, such as native async I/O, blocking thread-backed I/O, or platform-specific fiber I/O.
Adding that facade before a second backend exists would freeze speculative abstractions.

## Decision

Keep `AsyncFiberIO` concrete for now. Do not introduce a runtime-selectable I/O facade until at least two real backends
exist with enough shared behavior to justify a common API.

## Consequences

The current API remains explicit and easy to reason about: it depends on `Fibers` and `Async`, wraps caller-owned
runtime objects, and exposes their constraints directly. Backend polymorphism is deferred, so future backends may require
API changes once their real requirements are known.

## Confirmation

A change preserves this decision when new `AsyncFibers` features extend the concrete bridge, do not add an abstract
backend-selection layer without a second implemented backend, and continue documenting the wrapped `AsyncEventLoop`
relationship.

## Related

- [ASYNCFIBERS-0001 - Keep AsyncFibers as the bridge from Fibers to Async](asyncfibers-0001-keep-asyncfiber-as-the-bridge-from-fibers-to-async.md)
- [ASYNCFIBERS-0002 - Keep AsyncEventLoop owner-thread affine with bounded command posting](asyncfibers-0002-keep-asynceventloop-owner-thread-affine-with-bounded-command-posting.md)
- [AsyncFibers architecture](asyncfibers-architecture.md)
