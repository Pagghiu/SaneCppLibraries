---
name: sane-containers-reflection
description: Bridge Sane containers into reflection and serialization workflows. Use when SC::Vector, SC::String, SC::Array, SC::VectorMap, or similar container types must be reflected or serialized.
---

# Sane Containers Reflection

## Use This Skill

Use this skill when a reflected struct contains Sane container types and the reflection or serialization stack needs to understand them.

## What To Do

- Start with [Documentation/Libraries/ContainersReflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/ContainersReflection.md).
- Inspect [Tests/Libraries/Containers](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/Containers) for container shapes that show up in reflection or serialization.
- Include this bridge alongside `Reflection`, `SerializationBinary`, or `SerializationText` whenever a reflected model stores Sane containers.
- Treat this skill as a support layer, not as the primary user goal.

## What To Check

- Whether the type uses `SC::Vector`, `SC::String`, `SC::Array`, or a map-like container.
- Whether the consumer is reflection, binary serialization, or text serialization.
- Whether the field types already come from another skill and only need the adapter header.

## Reference

- [Container bridge notes](references/container-bridge.md)
