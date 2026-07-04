# HASHING-0002 - Keep Hashing State Inline and Non-Movable

Status: Accepted
Date: 2026-07-04

## Context

Native hashing APIs need per-operation state, but library code must not hide heap allocation or expose platform handles in public headers. Moving or copying a live native hash context could invalidate backend resources or duplicate ownership.

## Decision

`SC::Hashing` stores backend state inline in fixed-size storage or direct file descriptors and deletes copy and move operations. Callers create a hash object, select an algorithm with `setType`, stream bytes with `add`, and finalize with `getHash`.

## Consequences

The public type remains allocation-free and simple to use, but its size must accommodate supported backend state. Backend size changes require updating static assertions or storage. Hashing objects cannot be copied or moved; callers must manage their lifetime directly.

## Confirmation

A change preserves this decision when `SC::Hashing` contains no owning heap pointer, public headers do not expose native backend types, copy and move remain deleted, backend implementations validate inline storage size and alignment, and C bindings continue to fit the C opaque storage.

## Related

- [Hashing public API](../../Libraries/Hashing/Hashing.h)
- [Hashing native backends](../../Libraries/Hashing/Hashing.cpp)
- [HASHING-0001 - Wrap native OS hash APIs instead of bundling hash implementations](hashing-0001-wrap-native-os-hash-apis-instead-of-bundling-hash-implementations.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
