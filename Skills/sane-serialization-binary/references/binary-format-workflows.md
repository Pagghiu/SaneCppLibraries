# Binary Serialization Workflows

Use this reference when an agent needs a compact binary persistence path.

## Read First

- [Documentation/Libraries/SerializationBinary.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationBinary.md)
- [Documentation/Libraries/Reflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Reflection.md)
- [Tests/Libraries/SerializationBinary/SerializationBinaryTest.cpp](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationBinary/SerializationBinaryTest.cpp)

## Main Entry Points

- `SC::SerializationBinary::write`
- `SC::SerializationBinary::loadExact`
- `SC::SerializationBinary::loadVersioned`

## Decision Guide

- Use `write` for the binary output path.
- Use `loadExact` when the producer and consumer share the same schema version.
- Use `loadVersioned` when schema drift must not lose data.
- Prefer packed types when you want the fast path.

## Caveats

- The binary serializer is not streaming.
- Include the containers bridge if the reflected model stores Sane container types.
- Keep versioning expectations explicit in the skill prompt or reference route.

## Good Prompts

- Save a reflected config structure to disk.
- Load a previous binary save file after fields were added or moved.
- Explain whether a type can use the packed fast path.
