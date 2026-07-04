# MEMORY-0001 - Keep Memory as the Explicit Allocation Boundary

Status: Accepted
Date: 2026-07-04

## Context

Sane C++ Libraries treats hidden allocation as an architectural hazard. Some facilities still need allocation, allocator tracking, growable buffers, virtual memory, or owned string storage. If those facilities live in foundational or string-processing libraries, depending on those libraries no longer tells users whether allocation-capable code has entered their build.

## Decision

`Memory` is the explicit home for allocation-capable primitives such as `Memory`, `MemoryAllocator`, `FixedAllocator`, `Globals`, `VirtualMemory`, `Segment`, `Buffer`, `String`, and `SmallString`. Libraries that do not directly or indirectly depend on `Memory` should be understood as not requiring runtime dynamic allocation from Sane-owned code.

## Consequences

The dependency graph communicates allocation risk more clearly. Foundational and low-level libraries can stay small and allocation-free. Some types that might look conceptually related to other domains, especially owned strings, live in `Memory` because their defining behavior is ownership and allocator policy.

## Confirmation

A change preserves this decision when allocation-capable storage, allocator configuration, and owned Sane string storage remain in `Memory`; dependency reports keep `Memory` as the visible allocation-capable dependency; and non-allocation libraries do not include `Memory` merely for convenience.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [Memory documentation](../../Documentation/Libraries/Memory.md)
- [Project principles](../../Documentation/Pages/Principles.md)
