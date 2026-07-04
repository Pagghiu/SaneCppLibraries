# PROCESS-0001 - Keep Process Arguments and Environment in Explicit Arenas

Status: Accepted
Date: 2026-07-04

## Context

Launching a child process requires platform-specific command and environment layouts. Those layouts can grow with argument count, native path encoding, inherited environment size, and CI runner configuration. At the same time, Process must keep allocation visible and avoid depending on Strings or Memory just to format launch data.

## Decision

`Process` stores formatted command arguments and child environment entries in explicit native writable arenas. The default constructor uses inline storage, and callers that need more space pass command and environment spans. `StringsArena` and `EnvironmentTable` build the platform-specific views over that storage, while environment inheritance and overrides remain explicit API choices.

## Consequences

Process launch setup can fail with a normal `Result` when caller-provided storage is insufficient. Large environments require a larger caller-owned arena instead of hidden allocation. The implementation carries some platform-specific formatting complexity, but the library remains predictable and keeps dependency pressure low.

## Confirmation

A change preserves this decision when command and environment formatting uses inline or caller-provided storage, storage exhaustion is reported through `Result`, environment inheritance remains explicit, and Process does not gain a dependency on Strings, Memory, Containers, or STL containers for launch formatting.

## Related

- [Process documentation](../../Documentation/Libraries/Process.md)
- [Process public API](../../Libraries/Process/Process.h)
- [Process environment tests](../../Tests/Libraries/Process/ProcessTest.cpp)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0012 - Support bring-your-own containers](../Global/sc-0012-support-bring-your-own-containers.md)
