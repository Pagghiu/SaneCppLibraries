# Cryptography Architecture

## Purpose

Cryptography provides a small, allocation-free adapter over native secure random, authenticated encryption, legacy
block encryption, message authentication, and key derivation primitives. It supplies mechanisms and capability
reporting, not protocol policy or key management.

## Architectural Shape

`SC::Cryptography` groups stateless `Random` and `Hkdf` operations with non-copyable, non-movable `Aead`, `Cipher`, and
`Hmac` session objects. Native state lives in fixed `OpaqueObject` storage. Public inputs and outputs are borrowed spans;
no operation owns caller data or grows storage.

Apple uses CommonCrypto, Windows uses CNG, and Linux uses `getrandom()` and `AF_ALG`. CBC buffering and PKCS#7 handling
are platform-neutral so native backends process only aligned raw CBC blocks. HKDF is the RFC 5869 composition over the
library's native HMAC adapter.

## Boundaries

The library owns primitive invocation, fixed state, capability discovery, buffer validation, and lifecycle cleanup. It
does not own certificates, public-key cryptography, password hashing, nonce allocation, persistent keys, protocol
framing, constant-time comparison, or application authorization decisions.

System headers and platform handles remain in `Cryptography.cpp`. Common guarded headers are inlined source fragments,
not a Foundation or Common library dependency.

## Security Posture

AES-GCM is preferred. AES-CBC is explicitly legacy and unauthenticated. Authentication failures do not report plaintext
and clear the supplied output span. Native key-object buffers, CBC pending blocks, HMAC state, and HKDF intermediates are
cleared when their sessions end where the library owns those bytes. Draft status remains visible until platform coverage,
documentation, and external review justify promotion.

## Explicitly Excluded Targets

- Bundled algorithm implementations or third-party crypto dependencies.
- A generic pluggable algorithm registry.
- Bespoke encrypt-then-MAC protocol construction.
- Hidden heap allocation or movable native sessions.
- A claim that buffer clearing is equivalent to process-wide secure memory management.

## Sources

- [Cryptography documentation](../../Documentation/Libraries/Cryptography.md)
- [Cryptography public API](../../Libraries/Cryptography/Cryptography.h)
- [Cryptography native backends](../../Libraries/Cryptography/Cryptography.cpp)
- [Cryptography tests](../../Tests/Libraries/Cryptography/CryptographyTest.cpp)
- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0002](cryptography-0002-prefer-aead-and-keep-cbc-explicitly-legacy.md)

## Decision Log

- [CRYPTOGRAPHY-0001 - Wrap native symmetric cryptography with capability reporting](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0002 - Prefer AEAD and keep CBC explicitly legacy](cryptography-0002-prefer-aead-and-keep-cbc-explicitly-legacy.md)
