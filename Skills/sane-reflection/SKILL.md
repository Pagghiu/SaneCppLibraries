---
name: sane-reflection
description: Describe Sane C++ structs and types for serialization workflows. Use when an agent needs reflection metadata, packed struct handling, schema generation, member tags, or help preparing data types for binary or text serialization.
---

# Sane Reflection

## Use This Skill

Use this skill when a task needs compile-time reflection for a user-defined type, especially before binary or text serialization.

## What To Do

- Start with [Documentation/Libraries/Reflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Reflection.md).
- Inspect [Tests/Libraries/Reflection/ReflectionTest.cpp](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/Reflection/ReflectionTest.cpp) for supported macros and packed versus unpacked examples.
- Prefer `SC_REFLECT_STRUCT_VISIT`, `SC_REFLECT_STRUCT_FIELD`, and `SC_REFLECT_STRUCT_LEAVE` when macro-based reflection is the clearest fit.
- Treat reflection as a compile-time schema builder, not as runtime RTTI.
- Keep reflected members limited to supported shapes and avoid pointers or references.

## What To Check

- Whether the type is packed or unpacked.
- Whether nested structs or arrays need schema links.
- Whether the data will later be consumed by binary or text serialization.
- Whether `sane-containers-reflection` is also needed for container fields.

## Reference

- [Reflection macro and schema notes](references/reflection-macros-and-shapes.md)
