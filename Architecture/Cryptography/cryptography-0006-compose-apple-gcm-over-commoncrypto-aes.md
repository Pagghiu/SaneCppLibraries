# CRYPTOGRAPHY-0006 - Compose Apple GCM over CommonCrypto AES

Status: Accepted
Date: 2026-08-20

## Context

Apple exposes AES-GCM through CryptoKit, but using it requires a mixed Swift/C++ build that is incompatible with the
standalone C++ library contract. Public CommonCrypto does expose AES block encryption, while GCM itself is a specified
composition of counter-mode encryption and GHASH. Leaving Apple without AES-GCM creates a material capability gap for
record protocols.

## Decision

The private Apple adapter implements the narrowly required SP 800-38D GCM composition over CommonCrypto AES encryption.
It supports only the existing interface: AES-128 and AES-256 keys, 96-bit nonces, complete 128-bit tags, one-shot
operations, and exact in-place buffers. GHASH uses fixed-iteration mask-based multiplication without secret-indexed
tables. Opening authenticates ciphertext before decrypting it.

This is a specific exception to CRYPTOGRAPHY-0001's exclusion of bundled algorithm implementations. It does not permit a
portable AES implementation, a generic mode framework, variable nonce lengths, truncated tags, or third-party crypto.

## Consequences

Apple gains the same AES-GCM interface as Windows and Linux while keeping C++-only and single-file builds. Correctness
and constant-time behavior of GHASH become library responsibilities, although AES remains native. The simple portable
GHASH implementation favors auditability over hardware-specific acceleration.

## Confirmation

The decision is preserved when shared NIST vectors pass on every backend, corruption never releases plaintext, the
Apple adapter uses public CommonCrypto AES calls only, GHASH has fixed control flow and memory access independent of key
and message contents, and standalone amalgamation builds require no Swift or additional dependency.

## Related

- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0005](cryptography-0005-keep-swift-interop-out-of-the-core-backend.md)
- [Apple AES-GCM implementation plan](../../Plans/Cryptography/apple-aes-gcm.md)
- [NIST SP 800-38D](https://doi.org/10.6028/NIST.SP.800-38D)
