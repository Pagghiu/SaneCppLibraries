# Container Selection Guide

## What To Teach First

- `SC::Vector` for the general dynamic container case.
- `SC::Array` when heap allocation must never happen.
- `SC::SmallVector` when inline storage can reduce allocations.
- `SC::VectorMap`, `SC::VectorSet`, and `SC::ArenaMap` for compact associative needs.
- The dependency on `Memory` for allocator-aware behavior.

## Best Files To Inspect

- `Libraries/Containers/Vector.h`
- `Libraries/Containers/Array.h`
- `Libraries/Containers/VectorMap.h`
- `Libraries/Containers/VectorSet.h`
- `Libraries/Containers/ArenaMap.h`
- `Libraries/Containers/Vector.h` for `SmallVector`

## Best Examples

- `Tests/Libraries/Containers/VectorTest.cpp`
- `Tests/Libraries/Containers/ArrayTest.cpp`
- `Tests/Libraries/Containers/SmallVectorTest.cpp`
- `Tests/Libraries/Containers/VectorMapTest.cpp`
- `Tests/Libraries/Containers/VectorSetTest.cpp`
- `Tests/Libraries/Containers/ArenaMapTest.cpp`

## Common Advice

- Pick the smallest container that still satisfies the capacity story.
- Call out whether growth can spill to the heap.
- Mention when the container integrates with `Memory`.
- Keep `Array` failure semantics visible because it can be full.

## Handoff

- Route owned-buffer questions to `memory`.
- Route global rules questions to `core-patterns`.
