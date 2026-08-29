# CRYPTOGRAPHY-0007 - Offer OpenSSL Alongside Linux AF_ALG

Status: Accepted
Date: 2026-08-30

## Context

Linux AF_ALG preserves the library's dependency-free, fixed-storage design and performs no userspace provider
allocation, but its AES-GCM interface has a 4096-byte AAD limit and depends on kernel availability and security fixes.
OpenSSL 3 provides an optimized userspace alternative with broader deployment coverage, but necessarily allocates
internal provider and operation state. Neither tradeoff should silently replace the other.

The OpenSSL implementation should also avoid Linux-specific assumptions in its cryptographic adapter so a future
macOS or Windows dynamic-library loader can reuse it if a concrete need justifies shipping OpenSSL there.

## Decision

`Backend::Native` remains the default. On Linux it uses `getrandom()` plus AF_ALG exactly as before. Existing default
constructors, `queryFeatures(Features&)`, and `Hkdf::derive()` retain native behavior and do not load OpenSSL.

Callers can explicitly select `Backend::OpenSSL` when querying features, constructing `Aead`, `Cipher`, or `Hmac`, and
deriving HKDF output. On Linux this backend loads the versioned `libcrypto.so.3` runtime and uses provider-aware EVP for
AES-GCM, raw AES-CBC, and HMAC. Missing OpenSSL or unavailable provider algorithms affect only the OpenSSL feature set.
There is no automatic fallback in either direction.

The private OpenSSL symbol table accepts a dynamic-library handle and resolver callback. Linux owns `dlopen`, `dlsym`,
and `dlclose`; the EVP adapter contains no Linux handle-loading logic. A future Apple or Windows implementation can add
its own loader while reusing the same ABI table and cryptographic adapter. OpenSSL remains unavailable on those
platforms until such an implementation is separately justified and tested.

OpenSSL necessarily allocates provider, algorithm, cipher/MAC context, process, and thread state internally.
Cryptography cannot redirect or prevent those allocations. Sane code still performs no C++ heap allocation and writes
only to caller-owned output spans, but an OpenSSL-backed operation is not allocation-free end to end.

## Consequences

Linux retains an allocation-free, dependency-free AF_ALG choice with all documented limitations and gains an explicit
userspace OpenSSL choice without OpenSSL headers or a link-time `libcrypto` dependency. Backend choice is visible in the
public interface and capability result instead of being controlled by implicit fallback or environment-dependent
priority.

The public enum and backend-taking overloads are portable, but only Linux currently implements OpenSSL. Older glibc
integrations can require the system `dl` library. OpenSSL configuration and FIPS policy are respected through the
process default library context.

## Confirmation

The decision is preserved when the complete known-answer, lifecycle, authentication-failure, HMAC, HKDF, fuzz, and AAD
tests pass independently for both Linux backends; default construction does not load OpenSSL; AF_ALG retains its
4096-byte AAD limit; OpenSSL accepts larger AAD; public headers expose no OpenSSL types; ABI declarations match OpenSSL
3 headers; single-file builds compile; and documentation states the allocation difference.

## Related

- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0004](cryptography-0004-bound-linux-af-alg-aead-associated-data.md)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [Linux AF_ALG deprecation](https://www.kernel.org/doc/html/next/crypto/userspace-if.html)
- [OpenSSL 3 EVP cipher interface](https://docs.openssl.org/3.0/man3/EVP_EncryptInit/)
