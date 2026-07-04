# SERIALIZATIONTEXT-0002 - Support Exact and Field-Name Versioned Text Reads

Status: Accepted
Date: 2026-07-04

## Context

Generated text can be consumed as a strict machine format, but users may also reorder fields or load older text that is missing fields. Binary versioning uses member tags, but text formats have stable field names available in the document.

## Decision

Serialization Text exposes exact and versioned text read paths. Exact reads expect the same field order produced by the writer, apart from whitespace accepted by the concrete format. Versioned reads scan object fields and match them to reflected members by field name.

## Consequences

Exact reads stay simpler and can be optimized independently, while versioned reads support reordered fields and missing data where the reflected type can tolerate it. Field names become part of the text compatibility surface, so renaming a reflected field can break text compatibility even when binary tags remain stable.

## Confirmation

A change preserves this decision when `SerializationJson::loadExact` and `SerializationJson::loadVersioned` remain separate APIs, exact tests fail on unsupported order/name changes, versioned tests cover reordered fields and versioned vector elements, and field-name matching stays in the versioned text reader.

## Related

- [SerializationJson public API](../../Libraries/SerializationText/SerializationJson.h)
- [Serialization Text versioned reader](../../Libraries/SerializationText/Internal/SerializationTextReadVersioned.h)
- [SerializationJson tests](../../Tests/Libraries/SerializationText/SerializationJsonTest.cpp)
- [REFLECTION-0002 - Store member tags, names, pointers, and offsets in schema input](../Reflection/reflection-0002-store-member-tags-names-pointers-and-offsets-in-schema-input.md)
