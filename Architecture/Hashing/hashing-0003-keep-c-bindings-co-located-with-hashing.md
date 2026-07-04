# HASHING-0003 - Keep C Bindings Co-Located with Hashing

Status: Accepted
Date: 2026-07-04

## Context

Hashing is useful from C code and from tooling that wants a stable C ABI. Keeping C bindings in a separate bindings area made ownership and test placement less direct, while the C ABI still needs to mirror Hashing's no-allocation inline state rule.

## Decision

Hashing's C bindings live inside the Hashing library. The C API exposes an opaque fixed-size `sc_hashing_t`, constructs `SC::Hashing` in that storage with placement construction, forwards operations to the C++ implementation, and destroys the object through `sc_hashing_close`.

## Consequences

The C API ships with the same single-file library and tests as the C++ API. The C opaque storage must remain large and aligned enough for `SC::Hashing`, so changes to the C++ object layout must be checked against C ABI storage. The C API does not introduce a separate allocation or backend implementation.

## Confirmation

A change preserves this decision when `HashingCBindings.*` remains owned by the Hashing library, C bindings forward to `SC::Hashing`, static assertions protect C/C++ layout compatibility, and C binding tests run through the Hashing test.

## Related

- [Hashing C bindings header](../../Libraries/Hashing/HashingCBindings.h)
- [Hashing C bindings implementation](../../Libraries/Hashing/HashingCBindings.cpp)
- [Hashing C bindings test](../../Tests/Libraries/Hashing/HashingCBindingsTest.c)
- [HASHING-0002 - Keep Hashing state inline and non-movable](hashing-0002-keep-hashing-state-inline-and-non-movable.md)
