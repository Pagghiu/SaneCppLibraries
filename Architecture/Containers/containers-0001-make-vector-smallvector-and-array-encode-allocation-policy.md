# CONTAINERS-0001 - Make Vector, SmallVector, and Array Encode Allocation Policy

Status: Accepted
Date: 2026-07-04

## Context

The project needs practical containers while preserving explicit allocation behavior. A single vector type with hidden storage policy would make it hard for callers and reviewers to know whether growth can allocate, spill from inline storage, or fail because capacity is fixed.

## Decision

The primary sequence containers encode allocation policy in their type. `Vector<T>` is allocation-capable through `Memory`. `SmallVector<T, N>` starts with inline storage and may spill to heap storage when `N` is exceeded. `Array<T, N>` has fixed inline storage and reports failure when the array itself cannot grow.

## Consequences

Callers choose the allocation behavior they want at construction sites and type declarations. APIs can accept `Vector<T>&` when heap-capable growth is acceptable and callers can pass `SmallVector<T, N>` where avoiding common temporary allocations is useful. Fixed-capacity use cases can use `Array<T, N>` and handle failure without heap growth.

## Confirmation

A change preserves this decision when the three container families keep distinct allocation behavior, growth-capable operations expose failure through return values where possible, and tests continue to cover inline, heap-spill, and fixed-capacity paths.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [Containers documentation](../../Documentation/Libraries/Containers.md)
- [Vector](../../Libraries/Containers/Vector.h)
- [Array](../../Libraries/Containers/Array.h)
