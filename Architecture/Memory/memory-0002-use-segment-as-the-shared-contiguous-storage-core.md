# MEMORY-0002 - Use Segment as the Shared Contiguous Storage Core

Status: Accepted
Date: 2026-07-04

## Context

Several Sane types need contiguous storage with the same difficult behavior: size and capacity tracking, inline storage, heap growth, moving between inline and heap storage, copy and move operations, destructor handling, and allocator selection. Reimplementing that behavior separately in each owning buffer or container would duplicate bugs and make allocation behavior harder to audit.

## Decision

`Memory::Segment` is the shared storage core for contiguous owning storage. It owns the common header, size/capacity accounting, allocator selection, inline-data transition, and growth behavior. Type-specific storage such as `Buffer`, `String`, `Vector`, `SmallVector`, and `Array` layers policy and element operations on top through traits or derived types.

## Consequences

Fixes to storage growth and movement behavior concentrate in one implementation. Containers intentionally depend on `Memory` for this shared substrate. `Segment` is therefore more constrained than an ordinary helper: layout, inline storage behavior, and trivial/non-trivial element hooks must remain compatible with all consuming types.

## Confirmation

A change preserves this decision when contiguous owning Sane containers continue to share `Segment` behavior instead of growing independent storage engines, tests cover both trivial and non-trivial element movement, and changes to `Segment` are reviewed against `Buffer`, `String`, `Vector`, `SmallVector`, and `Array` behavior.

## Related

- [Segment](../../Libraries/Memory/Segment.h)
- [Buffer](../../Libraries/Memory/Buffer.h)
- [Containers Vector](../../Libraries/Containers/Vector.h)
- [CONTAINERS-0002 - Build Containers on Memory::Segment instead of STL-like independent implementations](../Containers/containers-0002-build-containers-on-memory-segment-instead-of-stl-like-independent-implementations.md)
