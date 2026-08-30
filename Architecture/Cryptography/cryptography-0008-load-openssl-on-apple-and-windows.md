# CRYPTOGRAPHY-0008 - Load OpenSSL on Apple and Windows

Status: Accepted
Date: 2026-08-30

## Context

CRYPTOGRAPHY-0007 introduced an explicit OpenSSL 3 backend on Linux and separated symbol resolution from Linux dynamic
loading. Apple and Windows retained only their native backends even when an application already shipped or installed
OpenSSL 3. Supporting the same explicit choice across platforms should not duplicate EVP cryptographic logic, add a
link-time dependency, weaken dynamic-library search policy, or change the native default.

## Decision

The fixed-storage OpenSSL AEAD, cipher, and HMAC session adapter is shared by Apple, Windows, and Linux. Each platform
supplies only a private runtime loader and symbol resolver. Apple probes the versioned `libcrypto.3.dylib` through normal
dynamic-loader paths and conventional Homebrew or MacPorts locations. Windows uses `LoadLibraryEx` with
`LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` and probes architecture-specific OpenSSL 3 DLL names. Linux retains its exact
`libcrypto.so.3` probe.

Every loaded candidate must expose the complete required symbol set and report OpenSSL major version 3. Failure closes
that candidate and continues probing; an entirely unavailable OpenSSL runtime produces an empty OpenSSL capability set.
There is no automatic fallback between OpenSSL and the native CommonCrypto, CNG, or AF_ALG adapters.

`Random::fill()` remains native on every platform because backend selection applies only to AEAD, cipher, HMAC, and
HKDF. OpenSSL continues to own and allocate its internal provider and operation state as documented by
CRYPTOGRAPHY-0007.

## Consequences

Applications can use one backend-selection interface on all supported platforms and can bundle OpenSSL beside a
Windows executable or install it through a conventional macOS package manager. Sane C++ still has no OpenSSL header or
link-time dependency. Native construction does not invoke a loader and remains the default.

macOS has no system OpenSSL 3 guarantee, and Windows installations use several conventional DLL names. Capability
queries therefore remain authoritative. The loader deliberately does not recursively discover arbitrary package
manager prefixes or search the Windows working directory.

## Confirmation

The shared known-answer, lifecycle, authentication-failure, HMAC, HKDF, fuzz, and AAD tests must pass through OpenSSL on
Apple, Windows, and Linux when a suitable runtime is present. Native suites, missing-runtime capability behavior,
single-file builds, and public-header isolation must continue to pass independently.

## Related

- [CRYPTOGRAPHY-0007](cryptography-0007-offer-openssl-alongside-linux-af-alg.md)
- [OpenSSL installation and shared-library notes](https://github.com/openssl/openssl/blob/master/INSTALL.md)
- [Windows dynamic-link library search order](https://learn.microsoft.com/windows/win32/dlls/dynamic-link-library-search-order)
