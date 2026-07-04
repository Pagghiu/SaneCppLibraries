# AWAIT-0001 - Wrap Caller-Owned AsyncEventLoop Instead Of Owning A Coroutine Runtime

Status: Accepted
Date: 2026-07-04

## Context

`Await` exists to provide C++20 coroutine syntax over `Async`, not to replace the lower-level event loop or introduce a separate runtime. Callback-style `Async` code and coroutine-style `Await` code must be able to coexist during migration.

## Decision

`AwaitEventLoop` wraps an existing caller-owned `AsyncEventLoop&` and uses a caller-provided `AwaitAllocator`. It forwards run modes to the underlying async loop and exposes awaiters that submit `AsyncRequest` objects to that same loop.

## Consequences

`Await` inherits `Async` lifetime, backend, cancellation, and platform behavior. Callers still own the underlying event loop and may mix callback and coroutine operations, but `Await` cannot hide loop setup, descriptor association, or stable-object rules.

## Confirmation

A change preserves this decision when `AwaitEventLoop` remains constructed from an `AsyncEventLoop&`, does not create or close the underlying loop, callback-style `Async` requests can share the loop with `Await` tasks, and documentation keeps the wrapper relationship explicit.

## Related

- [ASYNC-0001 - Keep AsyncRequest objects caller-owned and memory-stable](../Async/async-0001-keep-asyncrequest-objects-caller-owned-and-memory-stable.md)
- [Await documentation: design intent](../../Documentation/Libraries/Await.md)
- [AwaitEventLoop](../../Libraries/Await/Await.h)
