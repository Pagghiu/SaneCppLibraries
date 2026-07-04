# THREADING-0001 - Keep Threading as a Dependency-Free Native Primitive Layer

Status: Accepted
Date: 2026-07-04

## Context

Threading provides the basic user-space synchronization primitives that other libraries and applications build on. Depending on STL threading facilities or other Sane libraries would make these primitives less suitable as low-level building blocks and would complicate no-stdlib and single-file usage.

## Decision

Threading remains a dependency-free native primitive layer. It exposes `Thread`, `Mutex`, `ConditionVariable`, `EventObject`, `RWLock`, `Barrier`, `Semaphore`, `ThreadPool`, and narrow `Atomic` support through Sane primitives and platform-private implementations.

## Consequences

The library can be adopted standalone and used by higher-level code without pulling additional Sane libraries. Public headers carry opaque storage and explicit lifecycle rules instead of standard-library wrappers. Some primitive behavior is implemented in Sane code to preserve the dependency boundary.

## Confirmation

A change preserves this decision when Threading documentation reports no library dependencies, public APIs do not require STL threading or other Sane libraries, native OS headers remain private, and synchronization behavior is tested through the public primitives.

## Related

- [Threading documentation](../../Documentation/Libraries/Threading.md)
- [Threading public API](../../Libraries/Threading/Threading.h)
- [ThreadPool public API](../../Libraries/Threading/ThreadPool.h)
- [Atomic public API](../../Libraries/Threading/Atomic.h)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
