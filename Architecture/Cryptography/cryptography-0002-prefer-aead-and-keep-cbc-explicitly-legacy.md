# CRYPTOGRAPHY-0002 - Prefer AEAD and Keep CBC Explicitly Legacy

Status: Accepted
Date: 2026-08-05

## Context

Authenticated encryption is the safe default for new protocols, while AES-CBC remains necessary for compatibility with
some existing formats. Exposing both without policy cues would make the unauthenticated construction appear equivalent
to AES-GCM.

## Decision

Cryptography presents AES-GCM as the preferred encryption primitive and labels AES-CBC with PKCS#7 as legacy. CBC stays
available only as a raw compatibility mechanism: the library does not invent a combined CBC-and-MAC protocol. CBC uses
one platform-neutral caller-buffered streaming and padding state machine across all native block-cipher backends.

## Consequences

Callers using CBC must obtain authentication, IV rules, framing, and error behavior from an existing protocol. The
documentation warns about malleability, unpredictable IVs, and padding oracles. Platform-neutral buffering keeps chunk,
retry, overlap, and padding behavior consistent, at the cost of retaining one final decrypt block until `finish()`.

## Confirmation

The decision is preserved when documentation and API comments prefer AEAD, CBC tests cover arbitrary chunks and invalid
padding on every backend, input/output overlap remains rejected, and no unaudited bespoke CBC authentication scheme is
added.

## Related

- [Cryptography documentation](../../Documentation/Libraries/Cryptography.md)
- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
