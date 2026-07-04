# REFLECTION-0003 - Represent Schemas as Flat TypeInfo Arrays with Packed-Type Metadata

Status: Accepted
Date: 2026-07-04

## Context

Serializers need a compact schema representation that can be generated at compile time, embedded in binary payloads when needed, and consumed without allocation. They also need to distinguish recursively padding-free layouts from layouts that require member-wise traversal.

## Decision

Reflection compiles schemas into flat arrays of compact `TypeInfo` records. Parent records are followed by children, complex child types link to their full definition by index, and struct/array metadata records whether the type is recursively packed. Packed struct children are sorted by byte offset during schema compilation to support raw-layout binary serialization.

## Consequences

Schemas are small, constexpr-friendly, and easy to serialize as raw metadata, but they impose bounded counts and link-index limits. Binary serialization can optimize packed types, while non-packed types preserve visit order and member-wise traversal. Future schema expansion must account for the compact `TypeInfo` layout.

## Confirmation

A change preserves this decision when `Reflection::Schema::compile<T>()` still returns flat `TypeInfo` and name arrays, complex types are represented by schema links rather than heap-owned trees, packed metadata is computed recursively, and binary serializer tests still cover single-operation packed writes and versioned packed reads.

## Related

- [Reflection schema compiler](../../Libraries/Reflection/ReflectionSchemaCompiler.h)
- [Reflection TypeInfo](../../Libraries/Reflection/Reflection.h)
- [Reflection documentation: packed attribute](../../Documentation/Libraries/Reflection.md)
- [SERIALIZATIONBINARY-0002 - Optimize packed types as raw native-layout bytes](../SerializationBinary/serializationbinary-0002-optimize-packed-types-as-raw-native-layout-bytes.md)
