# CRYPTOGRAPHY-0012 - Load OpenSSL on NetBSD and FreeBSD

Status: Accepted
Date: 2026-11-10

## Context

NetBSD and FreeBSD have a libc entropy API and ship OpenSSL in the base system on the tested releases.
Their OpenSSL libraries have different SONAMEs, and the existing Linux AF_ALG backend does not apply.

## Decision

Use libc `getentropy` for both backend selections. Split output into at most 256 bytes per call, retry
interruptions and return the POSIX error on failure. Keep native symmetric capabilities unavailable on
BSD. Callers opt into OpenSSL for AES, HMAC and HKDF, which may allocate provider state internally.

Reuse the POSIX dynamic loader and existing OpenSSL 3 provider adapter. On BSD, try the base library
at `/usr/lib/libcrypto.so` and then the upstream `libcrypto.so.3` name. Validate required symbols and the
major version for each candidate and release any rejected handle. There is no header or link-time
dependency on OpenSSL. Use the existing runtime probes for advertised algorithms.

Validate AES key and CBC IV sizes at the public session entry points after resetting any previous session.
This keeps the same diagnostics when a provider is unavailable.

## Confirmation

Run the existing native and required OpenSSL suites on both BSDs with Debug and Release. Check larger
random buffers, runtime absence, rejected ABI and fallback selection. Compile the regenerated standalone
library on both BSDs and run the existing desktop regressions.

## OpenBSD extension

OpenBSD ships LibreSSL in its base system. The optional OpenSSL backend checks the upstream OpenSSL 3
library name, the OpenBSD OpenSSL 3.5 port's private library directory, and the normal loader search
path. It applies the same required-symbol and major-version checks to every candidate.
