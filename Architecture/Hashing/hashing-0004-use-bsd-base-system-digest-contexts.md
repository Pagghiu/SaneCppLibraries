# HASHING-0004 - Use BSD base-system digest contexts

Status: Accepted
Date: 2026-11-02

## Context

HASHING-0001 requires native digest facilities and HASHING-0002 keeps incremental state inline. NetBSD, FreeBSD, and OpenBSD provide streaming MD5, SHA-1 and SHA-256 with different names, return types, context types, and system libraries. Their headers also reuse generic type and macro names that conflict with OpenSSL when implementations are combined in a single translation unit.

## Decision

Use the base-system digest routines with caller-owned inline contexts: libc on NetBSD and OpenBSD, and `libmd` on FreeBSD. The FreeBSD tool bootstrap links `-lmd`. Check context sizes, alignment and digest sizes at compile time; split updates into bounded chunks where native lengths are narrower than `size_t`. Propagate native failures through the existing boolean API.

Keep native context declarations in a private namespace, derive function types from the system headers, bind the native C symbols, and restore the headers' macros and include guards. This permits the implementation to compose with other system headers in unity and single-file builds without exposing system types in the public header.

## Consequences

The C++ and C APIs retain their existing incremental behavior and fixed storage; no library-owned heap buffer or new inter-library dependency is introduced. FreeBSD applications linking Hashing need its base `libmd`. Backend availability and ABI are checked on each target system.

## Confirmation

Run existing C and C++ digest tests, reuse and empty-input checks, and generated single-file builds on all three BSDs. Exercise input larger than the native update count and native failures, verify native/OpenSSL header ordering and macro hygiene, and run existing-platform regressions. The implementation relies on each system's native digest interfaces, including the documented [FreeBSD digest interfaces](https://man.freebsd.org/cgi/man.cgi?query=sha&sektion=3&manpath=FreeBSD+15.0-RELEASE).

## Related

- [HASHING-0001 - Wrap native OS hash APIs](hashing-0001-wrap-native-os-hash-apis-instead-of-bundling-hash-implementations.md)
- [HASHING-0002 - Keep hashing state inline and non-movable](hashing-0002-keep-hashing-state-inline-and-non-movable.md)
- [Hashing documentation](../../Documentation/Libraries/Hashing.md)
