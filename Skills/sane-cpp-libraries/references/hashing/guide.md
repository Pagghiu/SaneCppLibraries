# Sane Hashing

## Quick Start

- Use this guide when the user needs a checksum or digest from bytes or a stream.
- Start with [references/hashing-streams-and-algorithms.md](references/hashing-streams-and-algorithms.md).

## Core Workflow

1. Inspect `Libraries/Hashing/Hashing.h` and `HashingCBindings.h`.
2. Use the streaming API when the input is already being read incrementally.
3. Use the algorithm that matches the user's compatibility or security need.
4. Keep the output format explicit so downstream code knows what digest it received.

## What To Emphasize

- Hashing is for bytes, not for generic structured objects.
- Streaming input is the common path.
- `MD5`, `SHA1`, and `SHA256` are the supported named algorithms in the library docs.

## Pitfalls

- Do not imply that the library provides general cryptographic policy.
- Do not confuse hashing with serialization or signing.
- Do not hide the byte ownership or streaming source when explaining usage.

## References

- [references/hashing-streams-and-algorithms.md](references/hashing-streams-and-algorithms.md)
- `Documentation/Libraries/Hashing.md`
- `Tests/Libraries/Hashing/*`
