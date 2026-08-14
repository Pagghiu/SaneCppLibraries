# CRYPTOGRAPHY-0001 - Wrap Native Symmetric Cryptography with Capability Reporting

Status: Accepted
Date: 2026-08-05

## Context

The project needs symmetric cryptography without STL, exceptions, hidden allocation, bundled algorithm implementations,
or third-party dependencies. Apple, Windows, and Linux expose different native primitive sets and availability rules.

## Decision

Cryptography wraps public native operating-system APIs behind fixed inline opaque storage. Apple uses CommonCrypto,
Windows uses CNG, and Linux uses `getrandom()` plus kernel `AF_ALG`. The public API exposes `queryFeatures()` so callers
select only primitives supported by the running backend. Missing capabilities return errors instead of falling back to
bundled or third-party implementations.

## Consequences

The library remains independently consumable and allocation-free in its own code, but primitive coverage differs by
platform and Linux availability depends on the running kernel. Backend differences require known-answer and lifecycle
tests on every supported operating system. Adding a platform or primitive requires a suitable native API or a new ADR.

## Confirmation

The decision is preserved when public headers contain no system types, backend state remains inline and non-movable,
`queryFeatures()` matches tested runtime behavior, and single-file builds introduce no external crypto dependency.

## Related

- [Cryptography documentation](../../Documentation/Libraries/Cryptography.md)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [SC-0009 - Isolate platform-specific implementations](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
