# CRYPTOGRAPHY-0012 - Load OpenSSL on BSD

Status: Accepted
Date: 2026-11-10

## Context

The tested BSDs have a libc entropy API. NetBSD and FreeBSD ship OpenSSL in the base system, while OpenBSD ships
LibreSSL and offers upstream OpenSSL as a port. Their libraries have different paths and SONAMEs, and the existing
Linux AF_ALG backend does not apply.

## Decision

Use libc `getentropy` for all BSD backend selections. Split output into at most 256 bytes per call, retry
interruptions and return the POSIX error on failure. Keep native symmetric capabilities unavailable on
BSD. Callers opt into OpenSSL for AES, HMAC and HKDF, which may allocate provider state internally.

Reuse the POSIX dynamic loader and existing OpenSSL 3 provider adapter. On NetBSD and FreeBSD, try the base library at
`/usr/lib/libcrypto.so` and then the upstream `libcrypto.so.3` name. On OpenBSD, try the upstream name, the port's private
library path, and the normal loader name. Validate required symbols and the major version for each candidate and release
any rejected handle. There is no header or link-time dependency on OpenSSL. Use the existing runtime probes for
advertised algorithms.

Validate AES key and CBC IV sizes at the public session entry points after resetting any previous session.
This keeps the same diagnostics when a provider is unavailable.

## Confirmation

Run the existing native and required OpenSSL suites on all three BSDs with Debug and Release. Check larger
random buffers, runtime absence, rejected ABI and fallback selection. Compile the regenerated standalone
library on all three BSDs and run the existing desktop regressions.
