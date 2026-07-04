# STRINGS-0001 - Keep Strings Allocation-free and Output Through GrowableBuffer

Status: Accepted
Date: 2026-07-04

## Context

String formatting, conversion, path composition, and console output often need variable-size output. If `Strings` owned that storage directly, it would depend on `Memory` and make many text operations allocation-capable by default. At the same time, callers need to use fixed buffers, Sane-owned strings, STL containers, or custom application containers as output targets.

## Decision

`Strings` remains allocation-free. Output-producing APIs write through `IGrowableBuffer` or `GrowableBuffer<T>` adapters so the caller chooses the storage and allocation policy. Concrete allocation-capable output types live outside `Strings`.

## Consequences

`Strings` can be used by low-level libraries without pulling in `Memory`. APIs are slightly more explicit because callers provide an output object or adapter, but fixed buffers, `StringPath`, `Memory::String`, STL adapters, and custom containers can all participate through the same small output-growth interface.

## Confirmation

A change preserves this decision when `Strings` dependency metadata stays free of `Memory`, output-producing string APIs accept caller-owned output or growable-buffer adapters, and storage exhaustion remains visible as a failed append, conversion, format, or path operation.

## Related

- [SC-0012 - Support bring-your-own containers](../Global/sc-0012-support-bring-your-own-containers.md)
- [COMMON-0007 - Keep IGrowableBuffer as the minimal output-growth adapter](../Common/common-0007-keep-igrowablebuffer-as-the-minimal-output-growth-adapter.md)
- [MEMORY-0003 - Keep owned String in Memory, not Strings](../Memory/memory-0003-keep-owned-string-in-memory-not-strings.md)
- [StringBuilder](../../Libraries/Strings/StringBuilder.h)
- [Path](../../Libraries/Strings/Path.h)
- [InteropSTL tests](../../Tests/InteropSTL)
