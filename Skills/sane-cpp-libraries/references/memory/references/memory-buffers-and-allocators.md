# Memory, Buffers, And Allocators

## What To Teach First

- `SC::Buffer` as the general growable byte buffer.
- `SC::Globals` as the central allocation policy hook.
- `SC::Memory` and `SC::VirtualMemory` as the storage and reservation layers.
- `SC::String` and `SC::SmallString` as owned string types that live here.
- `SC::Segment` as the storage block shared with containers.

## Best Files To Inspect

- `Libraries/Memory/Buffer.h`
- `Libraries/Memory/Globals.h`
- `Libraries/Memory/Memory.h`
- `Libraries/Memory/VirtualMemory.h`
- `Libraries/Memory/String.h`
- `Libraries/Memory/Segment.h`

## Best Examples

- `Tests/Libraries/Memory/BufferTest.cpp`
- `Tests/Libraries/Memory/GlobalsTest.cpp`
- `Tests/Libraries/Memory/StringTest.cpp`
- `Tests/Libraries/Memory/VirtualMemoryTest.cpp`

## Common Advice

- Be explicit that this is the allocation-aware layer.
- Explain when a buffer is caller-owned versus library-owned.
- Keep `String` ownership separated from `StringView` usage.
- Use `Globals` when the user needs to steer allocation globally or per thread.

## Handoff

- Route non-owning string questions to `strings`.
- Route container capacity questions to `containers`.
