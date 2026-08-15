# CRYPTOGRAPHY-0003 - Expose Idempotent Session Reset

Status: Accepted
Date: 2026-08-09

## Context

Destruction clears `Cipher` and `Hmac` native state, and successful finalization consumes a session. External lifecycle
adapters can nevertheless stop an operation before either event while the owning C++ object remains alive. Requiring
those adapters to destroy and reconstruct opaque sessions would duplicate lifetime machinery and make prompt cleanup
unreliable.

## Decision

`Cipher` and `Hmac` expose idempotent `reset()` operations. Reset releases native resources and clears library-owned
session state without moving or reconstructing the public object. `Hmac::reset()` preserves the selected hash family
but discards its key and accumulated input.

## Consequences

Stream adapters and other external lifecycles can promptly abandon active sessions. The public interface grows by one
cleanup operation per stateful streaming primitive, while normal `finish()` and `getMac()` behavior remains unchanged.

## Confirmation

The decision is preserved when repeated reset calls succeed, an active cipher cannot update until restarted after
reset, an active HMAC cannot add data until re-keyed after reset, and every backend clears the same state as destruction.

## Related

- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [ASYNCSTREAMS-0005](../AsyncStreams/asyncstreams-0005-adapt-cryptography-through-template-only-streams.md)
