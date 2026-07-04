# Hashing Architecture

## Purpose

Hashing provides a small streaming API for MD5, SHA1, and SHA256 over byte spans. It exists to give Sane C++ code and C callers a dependency-free wrapper over platform hashing facilities, not to implement or curate cryptographic algorithms.

## Architectural Shape

`SC::Hashing` is a non-copyable, non-movable state object. Callers select an algorithm with `setType`, stream data with `add`, and finalize into `Hashing::Result` with `getHash`. Platform state is stored inline in the public C++ object or as direct descriptors on Linux.

Native backends live in `Hashing.cpp`: CommonCrypto on Apple, WinCrypt on Windows, and `AF_ALG` on Linux. The C binding is co-located with the library and constructs the C++ object inside fixed opaque C storage.

## Boundaries

Hashing owns only streaming digest computation and its C binding. It does not own hex formatting, file reading, build cache policy, cryptographic protocol choices, key derivation, HMAC, encryption, or third-party algorithm implementations.

System headers and native API details must stay in `.cpp` implementation files or C binding headers where C ABI requires standard C includes. The C++ public header must not expose native OS handles or crypto structs.

## Similarities With Other Libraries

Hashing follows the global native-API preference and independent-consumption rules. Like platform abstraction libraries, it hides OS-specific code behind a small stable API. Like other no-allocation libraries, it stores fixed state and returns success/failure instead of throwing.

## Differences From Other Libraries

Unlike Reflection and SerializationBinary, Hashing is not schema- or template-driven. Unlike SerializationText, it does not need growable output because digest size is bounded. Unlike File or Process style wrappers, the public Hashing object is deliberately non-movable because native hash state ownership is backend-specific.

## Inspirations

The documented inspiration is the project-wide preference for readily available operating system APIs over third-party dependencies. Local documentation also notes Build as the main current consumer.

## Anti-Inspirations

Hashing is not OpenSSL, not a bundled crypto implementation suite, and not a general cryptography abstraction. Inference: it should not grow into a policy-heavy security library; algorithms should remain limited to project-supported digest needs unless a separate decision expands the scope.

## Architectural Choices

Keep algorithm state inline and backend-owned. Any new backend must preserve no hidden allocation and avoid exposing system headers in C++ public API.

Keep C bindings in the library so the C and C++ APIs are tested and distributed together. C opaque storage must be checked against `SC::Hashing` size and alignment.

Keep digest bytes as bytes. Formatting belongs to callers or string utilities.

## Explicitly Excluded Targets

- Third-party crypto libraries.
- Bundled software implementations of digest algorithms.
- Public OS crypto types in C++ headers.
- Hidden allocation or movable ownership wrappers around native state.
- Cryptographic protocols beyond raw hashing.

## Sources

- [Hashing documentation](../../Documentation/Libraries/Hashing.md)
- [Hashing public API](../../Libraries/Hashing/Hashing.h)
- [Hashing native backends](../../Libraries/Hashing/Hashing.cpp)
- [Hashing C bindings header](../../Libraries/Hashing/HashingCBindings.h)
- [Hashing C bindings implementation](../../Libraries/Hashing/HashingCBindings.cpp)
- [Hashing tests](../../Tests/Libraries/Hashing/HashingTest.cpp)
- [Hashing C bindings tests](../../Tests/Libraries/Hashing/HashingCBindingsTest.c)
- [SC-0008 - Prefer Native OS APIs Over Third-Party Dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [HASHING-0001 - Wrap Native OS Hash APIs Instead of Bundling Hash Implementations](hashing-0001-wrap-native-os-hash-apis-instead-of-bundling-hash-implementations.md)
- [HASHING-0002 - Keep Hashing State Inline and Non-Movable](hashing-0002-keep-hashing-state-inline-and-non-movable.md)
- [HASHING-0003 - Keep C Bindings Co-Located with Hashing](hashing-0003-keep-c-bindings-co-located-with-hashing.md)

## Decision Log

- [HASHING-0001 - Wrap native OS hash APIs instead of bundling hash implementations](hashing-0001-wrap-native-os-hash-apis-instead-of-bundling-hash-implementations.md)
- [HASHING-0002 - Keep Hashing state inline and non-movable](hashing-0002-keep-hashing-state-inline-and-non-movable.md)
- [HASHING-0003 - Keep C bindings co-located with Hashing](hashing-0003-keep-c-bindings-co-located-with-hashing.md)
