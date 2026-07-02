# SC-0011 - Make Allocation-Capable Facilities Explicit and Optional

Status: Accepted
Date: 2026-07-02

## Context

Some useful facilities need dynamic storage, growable buffers, virtual memory, or coroutine frame allocation. Treating all allocation as forbidden would make those facilities impossible. Treating allocation as ordinary convenience would break the project's central no-hidden-allocation promise.

## Decision

Allocation-capable facilities are allowed only when allocation is the explicit purpose of the library or an explicit caller-selected storage policy. Memory and Containers may expose allocation-capable types. Await may expose explicit coroutine-frame allocation policies. Other libraries use caller-provided storage, fixed buffers, spans, or explicit adapters when they need storage.

## Consequences

The project can provide practical allocation-aware tools without making allocation an invisible side effect of unrelated libraries. APIs may require more setup from callers, but the setup documents ownership and failure behavior. Library-specific ADRs should record intentional allocation-policy choices.

## Confirmation

A change preserves this decision when allocation-capable paths are named, configured, or typed explicitly; non-allocation libraries still work with caller-provided storage; and allocation failures are visible through `Result` or documented policy.

## Related

- [README No Allocations](../../README.md#no-allocations-)
- [Memory documentation](../../Documentation/Libraries/Memory.md)
- [Await documentation: Memory allocation](../../Documentation/Libraries/Await.md#memory-allocation)
- [SC-0001 - Library code must not hide dynamic allocation](sc-0001-no-hidden-allocation.md)
