---
name: sane-serialization-binary
description: Serialize reflected Sane C++ models to and from binary data. Use when an agent needs binary persistence, loadExact, loadVersioned, packed type fast paths, or help choosing binary over text serialization.
---

# Sane Serialization Binary

## Use This Skill

Use this skill when the task is to persist reflected data efficiently in binary form or to deserialize older or newer schema versions.

## What To Do

- Start with [Documentation/Libraries/SerializationBinary.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationBinary.md).
- Inspect [Tests/Libraries/SerializationBinary/SerializationBinaryTest.cpp](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationBinary/SerializationBinaryTest.cpp) and [Tests/Libraries/SerializationBinary/SerializationSuiteTest.h](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationBinary/SerializationSuiteTest.h).
- Use `SC::SerializationBinary::write` for output.
- Use `SC::SerializationBinary::loadExact` when the schema is unchanged.
- Use `SC::SerializationBinary::loadVersioned` when field additions, removals, or moves must remain compatible.
- Include `sane-containers-reflection` whenever the model stores Sane containers.

## What To Check

- Whether the type is packed and can use the fast memcpy-style path.
- Whether versioned loading is needed for old data.
- Whether the type should stay binary-only or should also be available in text form.
- Whether large payloads make the non-streaming binary design a concern.

## Reference

- [Binary serialization workflows](references/binary-format-workflows.md)
