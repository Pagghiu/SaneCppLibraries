# SC-0012 - Support Bring-Your-Own Containers

Status: Accepted
Date: 2026-07-02

## Context

Sane C++ provides its own containers and memory utilities, but it is not intended to force users into them or become an STL replacement. Users may already have `std::`, custom, arena-backed, embedded, or application-specific containers. Requiring Sane-owned containers everywhere would increase dependency pressure and make adoption less flexible.

## Decision

Library APIs should support caller-owned storage and bring-your-own-container patterns where practical. Sane containers remain available for users who want them, but lower-level libraries should prefer spans, string views, growable-buffer adapters, and explicit output storage over concrete owning container types.

## Consequences

Some APIs expose adapter contracts instead of convenient concrete types. This keeps allocation behavior visible, lets users use third-party or standard containers at the edge, and helps libraries avoid depending on Memory, Containers, or Strings just to produce output.

## Confirmation

A change preserves this decision when new output-producing APIs can work with caller-owned storage or explicit adapters, do not require Sane-owned containers unless the library purpose demands it, and keep STL interoperability outside core library implementation.

## Related

- [README No Allocations](../../README.md#no-allocations-)
- [InteropSTL tests](../../Tests/InteropSTL)
- [SC-0003 - Keep libraries independently consumable](sc-0003-keep-libraries-independently-consumable.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
