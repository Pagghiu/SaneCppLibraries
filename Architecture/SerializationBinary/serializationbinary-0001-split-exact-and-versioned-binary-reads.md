# SERIALIZATIONBINARY-0001 - Split Exact and Versioned Binary Reads

Status: Accepted
Date: 2026-07-04

## Context

Binary serialization has two different use cases. Data written by the same schema should load quickly with minimal checks. Data written by an older, newer, or otherwise different schema needs compatibility logic that can match fields and skip unknown data.

## Decision

Serialization Binary exposes separate exact and versioned read paths. `loadExact` reads data using the current reflected type layout and fails if trailing bytes remain. `loadVersioned` receives the source reflection schema and uses it to match struct members by `memberTag`, skip dropped fields, resize compatible arrays or vectors, and perform allowed primitive conversions.

## Consequences

Callers must choose the correct path or use the schema-prefixed helper that chooses for them. The exact path can remain simple and fast, while the versioned path carries compatibility options and schema-walking complexity. Compatibility behavior is explicit rather than hidden in every read.

## Confirmation

A change preserves this decision when `SerializationBinary::loadExact` and `SerializationBinary::loadVersioned` remain separate APIs, exact reads do not require a source schema, versioned reads do require source schema information, and tests continue covering reordered/dropped fields, array truncation, primitive conversion, and exact operation counts.

## Related

- [Serialization Binary public API](../../Libraries/SerializationBinary/SerializationBinary.h)
- [Serialization Binary documentation](../../Documentation/Libraries/SerializationBinary.md)
- [Serialization Binary suite tests](../../Tests/Libraries/SerializationBinary/SerializationSuiteTest.h)
- [REFLECTION-0002 - Store member tags, names, pointers, and offsets in schema input](../Reflection/reflection-0002-store-member-tags-names-pointers-and-offsets-in-schema-input.md)
