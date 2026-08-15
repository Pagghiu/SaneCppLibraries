# ASYNCSTREAMS-0005 - Adapt Cryptography Through Template-Only Streams

Status: Accepted
Date: 2026-08-09

## Context

CBC encryption and incremental HMAC naturally compose with bounded byte pipelines, but a direct `AsyncStreams`
dependency on `Cryptography` would weaken standalone consumption. A generic AEAD byte transform would also hide the
record framing, nonce allocation, tag placement, and authenticated-release policy required for safe decryption.

## Decision

`AsyncStreams` provides header-only cipher-transform and HMAC-sink templates over compatible session types. The
adapters include only `AsyncStreams.h`; callers instantiate them with `Cryptography::Cipher` or `Cryptography::Hmac`
only in translation units that explicitly use both libraries. Cipher output buffers must hold at least one 16-byte AES
block, and the adapter bounds each update so the session's maximum 15-byte output overhead always fits.

The library does not provide an AEAD byte stream. Any future AEAD adapter must first define an explicit record protocol
that withholds plaintext until authentication succeeds and specifies framing, nonces, tags, AAD, and bounded storage.

## Consequences

CBC and HMAC gain normal AsyncStreams backpressure, fixed queues, fan-out, error delivery, and destruction semantics
without allocation or a new library dependency. Template session types must provide the small `update` / `finish` /
`reset` or `add` / `reset` interface. CBC remains a legacy unauthenticated mechanism and is not promoted as a safe
default for new protocols.

## Confirmation

The decision is preserved when `CryptographyTransformStreams.h` does not include Cryptography, an AsyncStreams-only
test instantiates both adapters with fake sessions, cross-library tests cover CBC boundaries and HMAC fan-out,
single-file AsyncStreams builds remain independent, and no AEAD transform appears without a framing ADR.

## Related

- [ASYNCSTREAMS-0001](asyncstreams-0001-keep-asyncstreams-dependency-free-through-templated-async-adapters.md)
- [CRYPTOGRAPHY-0002](../Cryptography/cryptography-0002-prefer-aead-and-keep-cbc-explicitly-legacy.md)
- [Cryptography transform streams](../../Libraries/AsyncStreams/CryptographyTransformStreams.h)
