# THREADING-0002 - Require Explicit Thread and ThreadPool Lifetimes

Status: Accepted
Date: 2026-07-04

## Context

Thread execution and queued work need stable callable and task storage. Hiding that storage behind heap allocation would violate the project allocation model, while silently detaching or joining threads in destructors would hide important synchronization behavior.

## Decision

Threading requires explicit lifetimes. `Thread` must be joined or detached after it starts, and its destructor asserts if ownership is unresolved. `ThreadPool` does not allocate task nodes; callers provide `ThreadPoolTask` objects whose addresses remain stable until completion or destruction.

## Consequences

Thread users must make shutdown behavior visible in code. ThreadPool users must keep task storage alive and cannot queue temporary task objects. The API is stricter than allocation-backed task queues, but it makes ownership, synchronization, and failure modes reviewable.

## Confirmation

A change preserves this decision when started threads still require explicit join or detach, ThreadPool task storage remains caller-owned and stable, queued tasks expose invalid reuse through `Result` failures or assertions, and no hidden heap-backed task queue is introduced.

## Related

- [Threading public API](../../Libraries/Threading/Threading.h)
- [ThreadPool public API](../../Libraries/Threading/ThreadPool.h)
- [Threading tests](../../Tests/Libraries/Threading/ThreadingTest.cpp)
- [ThreadPool tests](../../Tests/Libraries/Threading/ThreadPoolTest.cpp)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0006 - Use explicit Result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
