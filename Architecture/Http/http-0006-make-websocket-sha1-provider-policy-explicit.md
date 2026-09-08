# HTTP-0006 - Make WebSocket SHA-1 Provider Policy Explicit

Status: Accepted
Date: 2026-09-08

## Context

RFC 6455 requires SHA-1 when deriving `Sec-WebSocket-Accept`. Windows and macOS provide native digest APIs. Linux can
usually load `libcrypto`, while its optional `AF_ALG` hashing interface may be unavailable or restricted. Silently
switching to embedded hashing hides a meaningful deployment choice, while requiring another SC library would break
Http's independent distribution.

## Decision

WebSocket handshake helpers accept an explicit SHA-1 mode. `Platform` is the default: it uses CNG on Windows,
CommonCrypto on macOS, and dynamically loaded OpenSSL EVP followed by AF_ALG on Linux. It returns `Result` failure when
no provider is usable. `SelfContained` selects Http's private allocation-free SHA-1 implementation deliberately.

Neither mode adds a link dependency or exposes system/provider declarations in public headers. SHA-1 remains limited to
the RFC 6455 handshake and is not presented as suitable for signatures, credentials, or other security decisions.

## Consequences

Applications can prefer maintained platform implementations or choose a fully self-contained distribution. Linux
deployments using `Platform` must provide a compatible `libcrypto` or AF_ALG. The embedded implementation carries a
small audit burden, so it is checked against standard vectors, block-boundary cases, chunked input, and independent
platform-provider results.

## Confirmation

Tests cover both public modes, known SHA-1 vectors including the million-`a` vector, every input length around two SHA-1
blocks, varied chunk sizes, provider agreement, and explicit platform-provider failure. Documentation records provider
order and failure behavior.

The known-answer cases come from [FIPS PUB 180-4](https://doi.org/10.6028/NIST.FIPS.180-4), and the platform path uses
the documented [OpenSSL EVP digest interface](https://docs.openssl.org/3.0/man3/EVP_DigestInit/),
[Windows CNG hashing](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcrypthash), or
[CommonCrypto digest API](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/CC_SHA1.3cc.html).

## Related

- [Http architecture](http-architecture.md)
- [Http documentation](../../Documentation/Libraries/Http.md)
- [SC-0003 - Keep Libraries Independently Consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0008 - Prefer Native OS APIs Over Third-Party Dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
