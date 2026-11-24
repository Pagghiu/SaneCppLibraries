# HTTP-0010 - Use BSD system SHA-1 providers for WebSocket handshakes

Status: Accepted
Date: 2026-11-02

## Context

HTTP-0006 makes the WebSocket handshake SHA-1 provider an explicit caller choice and keeps `Platform` as the default. NetBSD and FreeBSD base systems provide OpenSSL EVP, while OpenBSD provides the compatible LibreSSL EVP interface. Their library sonames differ from Linux. Selecting the embedded implementation silently would change the existing provider policy.

## Decision

On BSD, `Platform` dynamically loads the system EVP digest interface. NetBSD 11 tries `libcrypto.so.16`, FreeBSD 15 tries `libcrypto.so.35`, and both fall back to the unversioned library name. OpenBSD loads its unversioned base-system `libcrypto.so`. If no candidate exposes the required functions or computes the digest, return the existing provider-unavailable error. `SelfContained` remains an explicit, separate option.

## Consequences

The HTTP library gains no link-time crypto dependency or system declarations in public headers. Other releases must provide a compatible library under one of the tried names; this decision does not claim that every BSD installation has a provider. SHA-1 remains limited to the WebSocket handshake as described in HTTP-0006.

## Confirmation

Run the existing WebSocket handshake and provider-policy tests on all three BSDs in Debug and Release, including known-answer and provider-agreement vectors. Exercise a missing versioned library, all candidate libraries unavailable, and the explicit self-contained mode. Verify the standalone library and regression suites on existing platforms.

## Related

- [HTTP-0006 - Make WebSocket SHA-1 provider policy explicit](http-0006-make-websocket-sha1-provider-policy-explicit.md)
- [Http documentation](../../Documentation/Libraries/Http.md)
