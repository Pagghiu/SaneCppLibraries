# CRYPTOGRAPHY-0004 - Bound Linux AF_ALG AEAD Associated Data

Status: Accepted
Date: 2026-08-09

## Context

Patched Linux `algif_aead` implementations operate out of place and copy associated data into the receive scatterlist.
The allocation-free `Aead` API exposes AAD as read-only and intentionally requires ciphertext and plaintext buffers
only as large as their messages, so neither caller span is suitable as the kernel's writable AAD destination.

## Decision

The Linux AES-GCM backend supplies a fixed 4096-byte stack destination for returned AAD and rejects larger AAD before
submitting an operation. It accepts and closes a fresh AF_ALG operation socket for every `seal()` and `open()` while
retaining the keyed parent socket. Other platforms keep their native limits.

## Consequences

Linux gains allocation-free AES-128-GCM and AES-256-GCM with enough AAD capacity for record protocols such as TLS.
Applications needing more than 4096 bytes of AAD receive an explicit error and must choose another backend. Each message
also incurs one `accept()` and `close()` pair, which prevents operation state from leaking across one-shot messages.

## Confirmation

The decision is preserved when Linux known-answer tests cover AAD, exact in-place operation, empty and repeated messages,
wrong-tag output clearing, and the 4096/4097-byte boundary on a kernel where `algif_aead` is available.

## Related

- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [Cryptography documentation](../../Documentation/Libraries/Cryptography.md)
- [CVE-2026-31431](https://ubuntu.com/security/CVE-2026-31431)
