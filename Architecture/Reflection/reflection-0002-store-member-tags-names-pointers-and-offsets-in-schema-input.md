# REFLECTION-0002 - Store Member Tags, Names, Pointers, and Offsets in Schema Input

Status: Accepted
Date: 2026-07-04

## Context

Binary and text serializers need stable ways to match fields across schema versions. Field names are useful for textual formats and human-facing data, while compact integer tags are useful for binary compatibility. Serialization also needs a safe way to access members without reconstructing addresses from raw offsets.

## Decision

Reflected struct members provide four pieces of schema input: an integer member tag, a pointer-to-member, a field name string, and the member byte offset. Binary versioned serialization matches fields by member tag, text versioned serialization matches fields by name, and serializers access fields through the pointer-to-member.

## Consequences

Reflection declarations are more verbose, but binary and text compatibility can evolve independently. Field names can change without breaking binary data when tags remain stable, and tags can change without dictating textual field names. Offsets remain available for schema identity and packed-layout decisions without requiring serializers to reinterpret object storage as fields.

## Confirmation

A change preserves this decision when `SC_REFLECT_STRUCT_FIELD` and equivalent manual visitors still provide tag, pointer, name, and offset, versioned binary reads continue matching tags, versioned text reads continue matching names, and serializers do not replace pointer-to-member access with raw offset-based field access.

## Related

- [Reflection documentation: struct member info](../../Documentation/Libraries/Reflection.md)
- [Reflection TypeInfo](../../Libraries/Reflection/Reflection.h)
- [Serialization Binary versioned read](../../Libraries/SerializationBinary/Internal/SerializationBinaryReadVersioned.h)
- [Serialization Text versioned read](../../Libraries/SerializationText/Internal/SerializationTextReadVersioned.h)
