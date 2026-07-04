# SERIALIZATIONTEXT-0003 - Use IGrowableBuffer Transactional Output Instead of Strings/Memory

Status: Accepted
Date: 2026-07-04

## Context

Serialization Text must write variable-size output without forcing callers to use `SC::String`, `SC::Buffer`, `std::string`, or any allocation-capable container. Earlier dependency cleanup removed direct Memory and Strings dependencies, so the output abstraction must stay minimal and caller-owned.

## Decision

Serialization Text writes through `IGrowableBuffer` via `SerializationTextOutput`. Formatting begins by recording the current buffer size. If formatting fails, the output is resized back to that saved size; if formatting succeeds, the appended bytes remain.

## Consequences

Callers choose fixed, Sane-owned, STL, or custom storage adapters without Serialization Text depending on those containers. Failed writes avoid leaving partial formatted output appended to the caller buffer. The abstraction only handles byte output and rollback, not string ownership or null termination.

## Confirmation

A change preserves this decision when text writers accept growable-buffer adapters instead of concrete string or memory containers, failed formatting restores the original buffer size, and dependency reports do not show new SerializationText dependencies on Memory, Strings, Containers, or STL.

## Related

- [SerializationTextOutput](../../Libraries/SerializationText/Internal/SerializationTextOutput.h)
- [SerializationJson write](../../Libraries/SerializationText/SerializationJson.h)
- [COMMON-0007 - Keep IGrowableBuffer as the minimal output-growth adapter](../Common/common-0007-keep-igrowablebuffer-as-the-minimal-output-growth-adapter.md)
- [SC-0012 - Support bring-your-own containers](../Global/sc-0012-support-bring-your-own-containers.md)
