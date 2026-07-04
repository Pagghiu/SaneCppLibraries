# SERIALIZATIONBINARY-0003 - Allow Schema-Prefixed Payloads for Best-Effort Compatibility

Status: Accepted
Date: 2026-07-04

## Context

Versioned binary reads need the schema that produced the payload. Requiring every caller to store and route that schema separately makes compatibility harder to adopt, but always using the versioned reader would slow down matching-schema reads.

## Decision

Serialization Binary provides schema-prefixed helpers. `writeWithSchema` writes the reflected `TypeInfo` array before the object bytes. `loadVersionedWithSchema` reads that schema, compares it with the current schema, uses `loadExact` when they match, and falls back to `loadVersioned` when they differ.

## Consequences

Payloads can carry enough metadata for best-effort compatibility, at the cost of extra bytes and a format that assumes the serialized schema representation is readable by the consumer. Matching schemas still use the fast exact path. Non-prefixed payloads remain available for callers that manage schema externally or only need exact reads.

## Confirmation

A change preserves this decision when schema-prefixed write/read helpers remain available, matching serialized schemas dispatch to `loadExact`, mismatched schemas dispatch to `loadVersioned`, and the stored schema remains derived from `Reflection::Schema::compile<T>()`.

## Related

- [SerializationBinary::writeWithSchema](../../Libraries/SerializationBinary/SerializationBinary.h)
- [SerializationBinary::loadVersionedWithSchema](../../Libraries/SerializationBinary/SerializationBinary.h)
- [REFLECTION-0003 - Represent schemas as flat TypeInfo arrays with packed-type metadata](../Reflection/reflection-0003-represent-schemas-as-flat-typeinfo-arrays-with-packed-type-metadata.md)
- [SERIALIZATIONBINARY-0001 - Split exact and versioned binary reads](serializationbinary-0001-split-exact-and-versioned-binary-reads.md)
