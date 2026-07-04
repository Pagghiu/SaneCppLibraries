# CONTAINERS-0002 - Build Containers on Memory::Segment Instead of STL-like Independent Implementations

Status: Accepted
Date: 2026-07-04

## Context

`Containers` could implement standalone STL-like storage internals for each container. That would avoid a visible `Memory` dependency, but would duplicate allocation, inline storage, move, copy, destructor, and capacity-management rules already needed by `Buffer` and `String`.

## Decision

`Containers` builds its contiguous owning containers on `Memory::Segment` instead of separate STL-like storage engines. Container headers provide type-specific behavior, algorithms, and element-operation traits, while `Segment` supplies the shared contiguous storage implementation.

## Consequences

`Containers` intentionally depends on `Memory`, and that dependency is part of the library contract. Storage bugs and allocation-policy fixes are more local, but `Segment` changes must be validated against both `Memory` and `Containers`. Containers remain Sane-specific rather than STL wrappers or replacements.

## Confirmation

A change preserves this decision when `Containers` keeps `Memory` as its explicit storage dependency, avoids adding STL containers or standard-library runtime requirements to library code, and does not introduce parallel storage implementations for `Vector`, `SmallVector`, or `Array` without a superseding ADR.

## Related

- [MEMORY-0002 - Use Segment as the shared contiguous storage core](../Memory/memory-0002-use-segment-as-the-shared-contiguous-storage-core.md)
- [SC-0005 - Avoid STL, exceptions, and RTTI in library code](../Global/sc-0005-avoid-stl-exceptions-and-rtti-in-library-code.md)
- [Containers documentation](../../Documentation/Libraries/Containers.md)
- [Segment](../../Libraries/Memory/Segment.h)
- [Vector](../../Libraries/Containers/Vector.h)
