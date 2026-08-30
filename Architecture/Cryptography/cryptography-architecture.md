# Cryptography Architecture

## Purpose

Cryptography provides a small, fixed-storage adapter over secure random, authenticated encryption, legacy block
encryption, message authentication, and key derivation primitives. It supplies mechanisms and explicit backend
capability reporting, not protocol policy or key management.

## Architectural Shape

`SC::Cryptography` groups stateless `Random` and `Hkdf` operations with non-copyable, non-movable `Aead`, `Cipher`, and
`Hmac` session objects. Native state lives in fixed `OpaqueObject` storage. Public inputs and outputs are borrowed spans;
no operation owns caller data or grows storage.

Apple uses CommonCrypto, Windows uses CNG, and Linux defaults to `getrandom()` and `AF_ALG`. All three platforms also
offer an explicit OpenSSL 3 backend; it never replaces or silently falls back from the native adapter. CBC buffering and
PKCS#7 handling are platform-neutral so adapters process only aligned raw CBC blocks. HKDF is the RFC 5869 composition
over the selected HMAC adapter. Linux AF_ALG AES-GCM uses a fresh operation socket per message and bounds AAD at 4096
bytes so patched kernels can receive their required writable AAD copy without userspace allocation. Apple composes the narrowly
supported GCM construction over CommonCrypto AES because CryptoKit's generated C++ interface requires a separately
compiled Swift implementation and runtime, which is incompatible with the standalone C++ amalgamation contract.

The OpenSSL EVP session adapter and symbol table are platform-neutral. The symbol table receives a library handle plus
resolver from a private platform loader: Linux and Apple use `dlopen` / `dlsym`, while Windows uses `LoadLibraryEx` /
`GetProcAddress`. Loader filename policy varies privately without changing the public backend-selection interface.

## Boundaries

The library owns primitive invocation, fixed state, capability discovery, buffer validation, and lifecycle cleanup. It
does not own certificates, public-key cryptography, password hashing, nonce allocation, persistent keys, protocol
framing, a general-purpose constant-time comparison interface, or application authorization decisions.

System headers and platform handles remain in the private implementation. The copied OpenSSL ABI declarations require
neither OpenSSL headers nor a `libcrypto` link dependency. Common guarded headers are inlined source fragments, not a
Foundation or Common library dependency.

## Security Posture

AES-GCM is preferred. AES-CBC is explicitly legacy and unauthenticated. Authentication failures do not report plaintext
and clear the supplied output span. Native key-object buffers, CBC pending blocks, HMAC state, and HKDF intermediates are
cleared when their sessions end where the library owns those bytes. Draft status remains visible until sustained use,
platform coverage, documentation, and maintainer review justify promotion.

## Explicitly Excluded Targets

- Portable AES implementations, general-purpose bundled cryptography, or mandatory third-party crypto dependencies.
- A generic pluggable algorithm registry.
- Bespoke encrypt-then-MAC protocol construction.
- Sane-owned hidden heap allocation or movable provider sessions. OpenSSL's unavoidable internal allocation is explicit
  at backend selection and documented by CRYPTOGRAPHY-0007.
- A claim that buffer clearing is equivalent to process-wide secure memory management.

## Sources

- [Cryptography documentation](../../Documentation/Libraries/Cryptography.md)
- [Cryptography public API](../../Libraries/Cryptography/Cryptography.h)
- [Cryptography native backends](../../Libraries/Cryptography/Cryptography.cpp)
- [Cryptography tests](../../Tests/Libraries/Cryptography/CryptographyTest.cpp)
- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0002](cryptography-0002-prefer-aead-and-keep-cbc-explicitly-legacy.md)
- [CRYPTOGRAPHY-0003](cryptography-0003-expose-idempotent-session-reset.md)
- [CRYPTOGRAPHY-0004](cryptography-0004-bound-linux-af-alg-aead-associated-data.md)
- [CRYPTOGRAPHY-0005](cryptography-0005-keep-swift-interop-out-of-the-core-backend.md)
- [CRYPTOGRAPHY-0006](cryptography-0006-compose-apple-gcm-over-commoncrypto-aes.md)
- [CRYPTOGRAPHY-0007](cryptography-0007-offer-openssl-alongside-linux-af-alg.md)
- [CRYPTOGRAPHY-0008](cryptography-0008-load-openssl-on-apple-and-windows.md)

## Decision Log

- [CRYPTOGRAPHY-0001 - Wrap native symmetric cryptography with capability reporting](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0002 - Prefer AEAD and keep CBC explicitly legacy](cryptography-0002-prefer-aead-and-keep-cbc-explicitly-legacy.md)
- [CRYPTOGRAPHY-0003 - Expose idempotent session reset](cryptography-0003-expose-idempotent-session-reset.md)
- [CRYPTOGRAPHY-0004 - Bound Linux AF_ALG AEAD associated data](cryptography-0004-bound-linux-af-alg-aead-associated-data.md)
- [CRYPTOGRAPHY-0005 - Keep Swift interoperability out of the core backend](cryptography-0005-keep-swift-interop-out-of-the-core-backend.md)
- [CRYPTOGRAPHY-0006 - Compose Apple GCM over CommonCrypto AES](cryptography-0006-compose-apple-gcm-over-commoncrypto-aes.md)
- [CRYPTOGRAPHY-0007 - Offer OpenSSL alongside Linux AF_ALG](cryptography-0007-offer-openssl-alongside-linux-af-alg.md)
- [CRYPTOGRAPHY-0008 - Load OpenSSL on Apple and Windows](cryptography-0008-load-openssl-on-apple-and-windows.md)
