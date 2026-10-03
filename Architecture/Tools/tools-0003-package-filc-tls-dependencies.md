# TOOLS-0003 - Package Fil-C TLS and HTTP/2 Dependencies

Status: Accepted
Date: 2026-10-03

## Context

Fil-C's pointer ABI is incompatible with ordinary host C libraries. The native `curl-filc` package therefore needs TLS and HTTP/2 libraries built with the same Fil-C compiler, rather than loading host OpenSSL or nghttp2.

## Decision

Provide Linux ARM64 and x86-64 packages for pinned OpenSSL 3.6.5 and nghttp2 1.70.0. Apply a separately pinned Fil-C port patch to OpenSSL, and build OpenSSL against `zlib-filc`. Keep shared libraries, headers, receipts, and build state in the package roots. Cache identity includes the source pins, OpenSSL patch hash, compiler identity, profile, and relevant package identity. Validate package receipts and rerun ABI smoke programs on cache hits.

The package runtime paths are additive: Fil-C consumers use these libraries without replacing host system libraries. Other host platforms report the installers as unsupported.

## Consequences

Package installation requires native Fil-C, Perl/Autoconf build prerequisites, `make`, and `patch`. TLS and HTTP/2 remain optional runtime capabilities; a successful host-library build does not establish Fil-C ABI compatibility.

## Confirmation

Verify source and patch hashes, package-local exports and receipts, OpenSSL's version and SHA-256 vector plus TLS context lifetime, and nghttp2's version and serialized client SETTINGS frame. Smoke binaries must resolve package-local OpenSSL/zlib/nghttp2 libraries through their runtime search paths.

The experimental CI also runs SCTest through `Support/Scripts/RunFilCTLSFixture.sh`. Its bounded loopback server
and public test-only credentials exercise a trusted custom CA, required HTTP/2 status/body, wrong CA and hostname
rejection. The fixture uses per-run DSO copies because the SC runner prepares zlib again before execution. OpenSSL
Cryptography capabilities are mandatory in that run, including the existing backend and differential tests.
These checks establish functional ABI/protocol behavior; they are not the full upstream OpenSSL test suite,
constant-time validation, or a side-channel security proof. The port rebase is project-maintained, not an
upstream Fil-C 3.6.5 release.
