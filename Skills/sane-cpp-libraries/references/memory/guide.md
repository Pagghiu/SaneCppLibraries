# Sane Memory

## Overview

Use this guide when a user needs caller-owned buffers, allocator-aware objects, or the rules around Sane-owned strings. Keep the focus on storage ownership and allocation behavior, not on generic C++ memory theory.

## Use This Guide When

- A request needs a buffer strategy that stays explicit.
- A request asks where `SC::String` or `SC::SmallString` comes from.
- A request asks how to track allocations or define custom allocators.
- A request needs virtual memory or segment-style backing storage.
- A request is comparing `Memory` with `Containers` or `Strings`.

## Start Here

- Read [references/memory-buffers-and-allocators.md](references/memory-buffers-and-allocators.md).
- Inspect `Libraries/Memory/Buffer.h`, `Globals.h`, `VirtualMemory.h`, `Segment.h`, and `String.h`.
- Use `Tests/Libraries/Memory/BufferTest.cpp`, `GlobalsTest.cpp`, `StringTest.cpp`, and `VirtualMemoryTest.cpp`.

## Key Guidance

- Treat `Memory` as the library that makes explicit allocation possible.
- Remember that owned strings live here, not in `Strings`.
- Prefer caller-owned buffers when the user wants zero hidden allocation.

## Pitfalls

- Do not hide allocation behind convenience language.
- Do not route owned-string questions to `strings`.
- Do not treat `Memory` as a generic allocator abstraction without explaining the Sane ownership model.
