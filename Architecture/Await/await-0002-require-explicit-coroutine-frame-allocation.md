# AWAIT-0002 - Require Explicit Coroutine Frame Allocation

Status: Accepted
Date: 2026-07-04

## Context

C++ coroutine frames require allocation. Letting compiler-generated coroutine machinery fall back to hidden standard allocation would violate the project's allocation policy and make coroutine memory sizing invisible.

## Decision

`Await` requires explicit coroutine frame allocation through `AwaitAllocator`. The recommended mode is fixed caller-owned storage. Virtual-memory, malloc, and polymorphic allocator modes exist only as visible opt-in configurations, and allocation failures are reported through the coroutine task result path.

## Consequences

Every `AwaitEventLoop` setup must include an opened allocator, and examples/tests must size storage intentionally. In exchange, coroutine frame memory remains auditable, allocation-capable modes are visible at the call site, and diagnostics can report peak usage and failure sizes.

## Confirmation

A change preserves this decision when `AwaitTask::Promise` allocates frames through `AwaitAllocator`, there is no hidden standard allocation fallback, fixed storage remains the documented default, and tests cover allocation failure and allocator diagnostics.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [Await documentation: memory allocation](../../Documentation/Libraries/Await.md)
- [AwaitAllocator](../../Libraries/Await/Await.h)
