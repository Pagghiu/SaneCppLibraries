# FIBERS-0005 - Keep Logical Fiber State Out Of Thread-Local Storage

Status: Accepted
Date: 2026-07-07

## Context

`FiberTask` execution is not pinned to the OS thread that first starts the task. A task may yield, become ready again,
and later resume on a different worker thread after work stealing or when another thread drives the same
`FiberScheduler`. C++ `thread_local` variables and platform TLS are bound to OS threads, not to logical fiber tasks.

## Decision

Do not use C++ `thread_local` variables or platform TLS to store logical fiber task state. State that must survive fiber
suspension belongs in explicit task or caller-owned storage, such as captured task procedure state, caller-owned
objects, synchronization objects, or `FiberTask::userData()`.

## Consequences

Fiber migration across worker threads remains a valid scheduler behavior, and future work-stealing improvements do not
need to preserve accidental thread affinity for logical state correctness. Code that needs per-fiber state must make that
state explicit. OS-thread-local storage may still be used for true worker-thread implementation details, but not for
user-visible logical task state.

## Confirmation

A change preserves this decision when public APIs and examples do not encourage TLS-backed logical task state, tests for
work stealing or cross-thread resumption do not depend on stable OS-thread identity, and any internal TLS use is limited
to worker-thread implementation details that are safe when a fiber migrates.

## Related

- [Fibers documentation: Thread-Local State](../../Documentation/Libraries/Fibers.md)
- [FIBERS-0004 - Use bounded worker deques with intrusive global spill for work stealing](fibers-0004-use-bounded-worker-deques-with-intrusive-global-spill-for-work-stealing.md)
- [Fibers architecture](fibers-architecture.md)
