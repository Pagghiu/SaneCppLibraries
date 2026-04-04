# Sane Containers Reflection

## Use This Guide

Use this guide when a reflected struct contains Sane container types and the reflection or serialization stack needs to understand them.

## What To Do

- Start with `Documentation/Libraries/ContainersReflection.md`.
- Inspect `Tests/Libraries/Containers` for container shapes that show up in reflection or serialization.
- Include this bridge alongside `Reflection`, `SerializationBinary`, or `SerializationText` whenever a reflected model stores Sane containers.
- Treat this guide as a support layer, not as the primary user goal.

## What To Check

- Whether the type uses `SC::Vector`, `SC::String`, `SC::Array`, or a map-like container.
- Whether the consumer is reflection, binary serialization, or text serialization.
- Whether the field types already come from another skill and only need the adapter header.

## Reference

- [Container bridge notes](references/container-bridge.md)
