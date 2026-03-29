# Reflection Macro And Shape Notes

Use this reference when an agent needs the smallest useful summary of Sane reflection.

## Read First

- [Documentation/Libraries/Reflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Reflection.md)
- [Tests/Libraries/Reflection/ReflectionTest.cpp](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/Reflection/ReflectionTest.cpp)

## Key Rules

- Describe types externally through `SC::Reflection::Reflect<>`.
- Use the reflection macros for concise field lists when they match the type shape.
- Preserve member tags and field names when the type may be serialized across versions.
- Treat packed types as the fast path for binary serialization.
- Avoid pointers and references in reflected data.

## Useful Checks

- Ask whether the struct is expected to stay ABI stable.
- Ask whether field renames must remain compatible with older text or binary data.
- Ask whether nested `SC::Vector`, `SC::String`, or similar containers need the containers bridge.

## Good Prompts

- Describe a struct so the serializers can see it.
- Print or inspect the schema for a packed versus unpacked type.
- Add reflection metadata without changing the runtime data layout.
