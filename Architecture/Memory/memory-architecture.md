# Memory Architecture

## Purpose

`Memory` is the explicit allocation and owned-storage library for Sane C++ Libraries. Future changes must preserve the rule that a dependency on `Memory` is a clear signal that allocator-backed storage, allocation tracking, virtual memory, growable buffers, or owned strings may be involved.

## Architectural Shape

The library is shaped around a small set of deep modules. `Memory` provides raw allocation functions, `MemoryAllocator` and `FixedAllocator` provide allocator adapters, `Globals` selects global or thread-local allocator state, `VirtualMemory` owns virtual reservation/commit behavior, and `Segment` is the shared contiguous storage implementation used by `Buffer`, `String`, and `Containers`.

`Buffer`, `String`, and `SmallString` should stay thin storage policies over the shared machinery. New owning contiguous storage should first ask whether it can reuse `Segment` instead of adding another allocator-growth implementation.

## Boundaries

`Memory` owns allocation-capable Sane storage. It does not own non-owning primitives such as `Span`, `StringSpan`, or `StringPath`; those belong in `Common` when low-level libraries need them without allocation. It also does not own text algorithms, path algorithms, formatting, reflection policy, or general-purpose container algorithms.

Public interfaces should make allocation behavior visible through type choice, allocator configuration, or failure-returning operations. Convenience APIs must not make unrelated libraries depend on `Memory`.

## Similarities With Other Libraries

Like the rest of the project, `Memory` avoids STL containers, exceptions, and RTTI in library code, keeps public headers small, and reports recoverable storage failure through return values where the interface can do so. It uses Common primitives directly rather than depending on `Foundation`.

## Differences From Other Libraries

Unlike most libraries, `Memory` is allowed to allocate because allocation is its purpose. It is also the home for `String` and `SmallString`, even though they are string-shaped, because ownership and allocator policy dominate their architecture.

## Inspirations

The evidenced inspiration is the project-wide no-hidden-allocation model: allocation is acceptable only when it is the explicit purpose or selected policy. `FixedAllocator`, `Globals`, `VirtualMemory`, and `Segment` all follow that local Sane pattern.

## Anti-Inspirations

Inference: `Memory` is deliberately not a smart-pointer or ownership-framework library. Shared ownership and many tiny heap objects are discouraged by the project principles, so additions such as `SharedPtr` should require fresh design justification.

Inference: `Memory` should not become a hidden replacement for `Foundation`; moving primitives here merely because they are widely useful would make allocation risk ambiguous again.

## Architectural Choices

Keep `Memory` dependency-free except for Common fragments.

Keep `Segment` as the single implementation for contiguous owned storage unless a superseding ADR accepts another storage core.

Keep owned strings in `Memory` and expose string content through `StringSpan` views.

Keep allocator selection explicit enough that tests and callers can choose global, thread-local, fixed-buffer, or virtual-memory behavior intentionally.

## Explicitly Excluded Targets

Do not make `Memory` a general STL replacement.

Do not add hidden allocation to non-Memory libraries through helper includes.

Do not move `StringView`, `StringSpan`, `StringPath`, path logic, or formatting into `Memory`.

Do not add shared-ownership facilities without a new ADR that explains why the project principles no longer cover the case.

## Sources

- [Memory documentation](../../Documentation/Libraries/Memory.md)
- [Project principles](../../Documentation/Pages/Principles.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [MEMORY-0001 - Keep Memory as the explicit allocation boundary](memory-0001-keep-memory-as-the-explicit-allocation-boundary.md)
- [MEMORY-0002 - Use Segment as the shared contiguous storage core](memory-0002-use-segment-as-the-shared-contiguous-storage-core.md)
- [MEMORY-0003 - Keep owned String in Memory, not Strings](memory-0003-keep-owned-string-in-memory-not-strings.md)
- [Memory headers](../../Libraries/Memory)
- [Memory tests](../../Tests/Libraries/Memory)

## Decision Log

- [MEMORY-0001 - Keep Memory as the explicit allocation boundary](memory-0001-keep-memory-as-the-explicit-allocation-boundary.md)
- [MEMORY-0002 - Use Segment as the shared contiguous storage core](memory-0002-use-segment-as-the-shared-contiguous-storage-core.md)
- [MEMORY-0003 - Keep owned String in Memory, not Strings](memory-0003-keep-owned-string-in-memory-not-strings.md)
