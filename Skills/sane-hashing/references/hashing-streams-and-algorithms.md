# Hashing Streams And Algorithms

Use this reference when a user needs to hash byte streams or choose between the supported algorithms.

## Teach First

- The input is a byte stream or byte buffer.
- The output is a digest string or raw hash value depending on the caller.
- The library exposes `MD5`, `SHA1`, and `SHA256` in the public docs.

## Best Files To Inspect

- `Libraries/Hashing/Hashing.h`
- `Libraries/Hashing/HashingCBindings.h`
- `Tests/Libraries/Hashing/*`

## Good Advice To Give

- Prefer the streaming path when data already arrives in chunks.
- Mention the expected digest format before handing the value to another API.
- Route structured-object persistence to `sane-serialization-recipes` instead of treating hashing as a serializer.
