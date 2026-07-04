# Containers Architecture

## Purpose

`Containers` provides the Sane-owned container family for callers that want project-native containers with explicit storage policy. It should make allocation behavior visible through type choice rather than pretending that all containers have the same ownership and growth model.

## Architectural Shape

The central sequence types are `Vector<T>`, `SmallVector<T, N>`, and `Array<T, N>`. They share `Memory::Segment` behavior where possible and differ primarily in allocation policy: heap-capable, inline-then-heap, or fixed inline capacity. Associative helpers such as `VectorMap` and `VectorSet` build on vector-like storage rather than introducing heavier map machinery prematurely.

Container algorithms should stay small, header-only where the template surface requires it, and Sane-specific. They should use Common and Memory primitives, not STL containers or exception-based behavior.

## Boundaries

`Containers` owns Sane container types and their algorithms. It does not own allocator infrastructure, raw virtual memory, string formatting, string ownership, reflection metadata, or serialization specializations. Those remain in `Memory`, `Strings`, and `ContainersReflection` as appropriate.

Container APIs may depend on `Memory`, but other libraries should not be forced to depend on `Containers` merely to accept or produce lists. Prefer `Span`, caller-provided storage, `IGrowableBuffer`, or small adapters at lower-level library interfaces.

## Similarities With Other Libraries

Like other Sane libraries, `Containers` avoids STL, exceptions, RTTI, hidden dependencies, and hidden allocation. Its public behavior should be understandable from types and return values.

## Differences From Other Libraries

Unlike allocation-free libraries, `Containers` intentionally depends on `Memory` and may allocate when callers choose allocation-capable types. Unlike `Memory`, it owns typed object-container policy and algorithms rather than allocator mechanics.

## Inspirations

The evidenced inspiration is the project's bring-your-own-container model: Sane containers exist for users who want them, but lower-level libraries should not require them. The `Vector`/`SmallVector`/`Array` split is also inspired by the project-wide need to make storage policy explicit.

## Anti-Inspirations

Inference: `Containers` is deliberately not the C++ Standard Library with different names. It should not chase broad STL surface area, exception semantics, allocator traits, iterator categories, or hidden template machinery unless a concrete Sane use case demands it.

Inference: general hash/tree maps are not excluded forever, but adding them just to match common container catalogs would fight the current "add what is needed" shape.

## Architectural Choices

Keep `Vector`, `SmallVector`, and `Array` as the primary storage-policy vocabulary.

Keep contiguous owning storage on `Memory::Segment` instead of adding parallel storage cores.

Keep fixed-capacity failure visible in `Array` operations.

Keep maps and sets simple until an accepted design requires stronger lookup guarantees.

Keep `ContainersReflection` responsible for reflection and serialization specializations.

## Explicitly Excluded Targets

Do not make `Containers` a mandatory dependency for lower-level libraries that can use spans or adapters.

Do not introduce STL containers, exceptions, RTTI, or standard allocator protocols into library implementation.

Do not hide allocation behind container-looking APIs that do not communicate their storage policy.

Do not add reflection or serialization coupling directly to `Containers`.

## Sources

- [Containers documentation](../../Documentation/Libraries/Containers.md)
- [SC-0005 - Avoid STL, exceptions, and RTTI in library code](../Global/sc-0005-avoid-stl-exceptions-and-rtti-in-library-code.md)
- [SC-0012 - Support bring-your-own containers](../Global/sc-0012-support-bring-your-own-containers.md)
- [CONTAINERS-0001 - Make Vector, SmallVector, and Array encode allocation policy](containers-0001-make-vector-smallvector-and-array-encode-allocation-policy.md)
- [CONTAINERS-0002 - Build Containers on Memory::Segment instead of STL-like independent implementations](containers-0002-build-containers-on-memory-segment-instead-of-stl-like-independent-implementations.md)
- [Vector](../../Libraries/Containers/Vector.h)
- [Array](../../Libraries/Containers/Array.h)
- [VectorMap](../../Libraries/Containers/VectorMap.h)
- [Containers tests](../../Tests/Libraries/Containers)
- [InteropSTL tests](../../Tests/InteropSTL)

## Decision Log

- [CONTAINERS-0001 - Make Vector, SmallVector, and Array encode allocation policy](containers-0001-make-vector-smallvector-and-array-encode-allocation-policy.md)
- [CONTAINERS-0002 - Build Containers on Memory::Segment instead of STL-like independent implementations](containers-0002-build-containers-on-memory-segment-instead-of-stl-like-independent-implementations.md)
