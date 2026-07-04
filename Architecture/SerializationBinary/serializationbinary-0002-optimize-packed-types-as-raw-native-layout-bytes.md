# SERIALIZATIONBINARY-0002 - Optimize Packed Types As Raw Native-Layout Bytes

Status: Accepted
Date: 2026-07-04

## Context

Many reflected types contain only primitive fields with no padding at any level. Writing each member separately would preserve portability but would waste operations for data that is already laid out densely. The project values low overhead but must keep the trade-off visible.

## Decision

Serialization Binary serializes primitives and recursively packed structs, arrays, and vector elements as raw native-layout bytes. Non-packed data is serialized member-by-member or item-by-item. Packed deserialization writes all bytes for the object representation and does not invoke member constructors for those bytes.

## Consequences

Packed types can serialize with very few operations, but the binary format is tied to the native representation choices captured by the reflected schema, including type sizes and byte order. Callers who need portable interchange across incompatible platforms must not treat this optimized format as a canonical cross-platform wire format without additional constraints.

## Confirmation

A change preserves this decision when packed types still use `serializeBytes` fast paths, non-packed types still traverse reflected members, tests assert packed write/read operation counts, and documentation continues to describe the native-layout trade-off.

## Related

- [Serialization Binary exact read/write](../../Libraries/SerializationBinary/Internal/SerializationBinaryReadWriteExact.h)
- [Serialization Binary documentation: binary format](../../Documentation/Libraries/SerializationBinary.md)
- [Serialization Binary suite tests](../../Tests/Libraries/SerializationBinary/SerializationSuiteTest.h)
- [REFLECTION-0003 - Represent schemas as flat TypeInfo arrays with packed-type metadata](../Reflection/reflection-0003-represent-schemas-as-flat-typeinfo-arrays-with-packed-type-metadata.md)
