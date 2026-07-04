# HASHING-0001 - Wrap Native OS Hash APIs Instead of Bundling Hash Implementations

Status: Accepted
Date: 2026-07-04

## Context

Hashing needs MD5, SHA1, and SHA256 for project tooling and user code. Bundling algorithm implementations would add code to maintain and audit, while third-party dependencies conflict with the project's dependency model. Supported operating systems already expose hashing APIs.

## Decision

Hashing wraps native OS hashing APIs instead of bundling hash implementations. Apple builds use CommonCrypto, Windows builds use WinCrypt, and Linux builds use the kernel `AF_ALG` hash interface when available. Public headers expose only the Sane C++ API and keep system headers in implementation files.

## Consequences

The library stays small and dependency-free, but platform behavior and availability depend on OS facilities. Implementation code is platform-specific and must handle each backend's state and lifecycle rules. New algorithms should be added only when they can fit the same native-API and dependency constraints.

## Confirmation

A change preserves this decision when Hashing does not introduce third-party crypto code or public system headers, each supported platform backend uses native APIs, tests verify known MD5/SHA1/SHA256 digests, and unsupported platforms fail through the existing result path rather than hidden fallback dependencies.

## Related

- [Hashing documentation](../../Documentation/Libraries/Hashing.md)
- [Hashing public API](../../Libraries/Hashing/Hashing.h)
- [Hashing implementation](../../Libraries/Hashing/Hashing.cpp)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
