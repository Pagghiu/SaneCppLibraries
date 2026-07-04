# SerializationBinary Architecture

## Purpose

SerializationBinary converts reflected C++ objects to and from a compact binary representation. It is optimized for Sane C++ data structures and caller-owned buffers, not for becoming a universal platform-independent interchange format.

## Architectural Shape

SerializationBinary is header-only and built directly on Reflection. The exact path walks the current reflected type and reads or writes bytes through a minimal binary stream interface. The versioned path walks a supplied source schema, matches struct fields by member tag, skips unknown source data, and applies explicit compatibility options.

Packed types are a first-class optimization. When Reflection proves a type is recursively packed, SerializationBinary may read or write its native object representation in one operation. Schema-prefixed helpers prepend the flat Reflection schema and choose exact or versioned loading at read time.

## Boundaries

SerializationBinary owns the binary object representation, exact and versioned binary read/write traversal, schema-prefixed payload helpers, and binary compatibility options. It does not own reflection metadata definitions, text serialization, dynamic JSON documents, external storage allocation, or cross-endian/cross-ABI canonicalization.

Container storage adaptation belongs in reflection/serialization adapter libraries. Binary format changes must respect Reflection's schema contract instead of inventing a parallel metadata system.

## Similarities With Other Libraries

SerializationBinary follows the same explicit-storage pattern as SerializationText: callers provide buffers, failures return status, and concrete allocation-capable containers remain optional. Like Reflection, it uses template specialization to keep traversal static and single-file friendly.

## Differences From Other Libraries

Unlike SerializationText, SerializationBinary uses member tags for versioned compatibility and can rely on native memory layout for packed types. Unlike Hashing or platform libraries, it has no `.cpp` backend and no direct OS API boundary. Unlike Reflection, it owns byte movement and compatibility behavior, not schema discovery.

## Inspirations

No external binary serialization format is cited in local sources. The evidenced inspirations are the project's Reflection schema model, no-allocation principle, and the need for fast exact reads plus compatibility reads for evolving Sane C++ structs.

## Anti-Inspirations

SerializationBinary is not a self-describing universal wire protocol by default, not a schema registry, and not a byte-order-normalizing interchange format. Inference: it deliberately avoids Protobuf/FlatBuffers-style external schema languages because schemas are generated from `Reflect<T>` and kept inside the Sane C++ type system.

## Architectural Choices

Keep exact and versioned readers separate. Exact reads should stay simple and schema-blind; versioned reads should carry the schema and compatibility rules.

Keep packed-type raw byte optimization visible in docs and tests. Any change that weakens native-layout assumptions must either update the packed contract or introduce a separate portable mode with its own ADR.

Use schema-prefixed payloads only as a convenience format. Do not make all binary payloads carry schemas, and do not make schema-prefixed reads silently hide incompatible data loss beyond the existing options.

## Explicitly Excluded Targets

- Hidden allocation during serialization or deserialization.
- Canonical cross-platform byte order or ABI-independent binary layout.
- Textual formats or JSON token handling.
- A separate schema language independent of Reflection.
- Constructor-driven reconstruction for packed byte reads.

## Sources

- [Serialization Binary documentation](../../Documentation/Libraries/SerializationBinary.md)
- [SerializationBinary public API](../../Libraries/SerializationBinary/SerializationBinary.h)
- [SerializationBinary exact read/write](../../Libraries/SerializationBinary/Internal/SerializationBinaryReadWriteExact.h)
- [SerializationBinary versioned read](../../Libraries/SerializationBinary/Internal/SerializationBinaryReadVersioned.h)
- [SerializationBinary schema helper](../../Libraries/SerializationBinary/Internal/SerializationBinarySchema.h)
- [SerializationBinary tests](../../Tests/Libraries/SerializationBinary/SerializationSuiteTest.h)
- [REFLECTION-0002 - Store Member Tags, Names, Pointers, and Offsets in Schema Input](../Reflection/reflection-0002-store-member-tags-names-pointers-and-offsets-in-schema-input.md)
- [REFLECTION-0003 - Represent Schemas as Flat TypeInfo Arrays with Packed-Type Metadata](../Reflection/reflection-0003-represent-schemas-as-flat-typeinfo-arrays-with-packed-type-metadata.md)
- [SERIALIZATIONBINARY-0001 - Split Exact and Versioned Binary Reads](serializationbinary-0001-split-exact-and-versioned-binary-reads.md)
- [SERIALIZATIONBINARY-0002 - Optimize Packed Types As Raw Native-Layout Bytes](serializationbinary-0002-optimize-packed-types-as-raw-native-layout-bytes.md)
- [SERIALIZATIONBINARY-0003 - Allow Schema-Prefixed Payloads for Best-Effort Compatibility](serializationbinary-0003-allow-schema-prefixed-payloads-for-best-effort-compatibility.md)

## Decision Log

- [SERIALIZATIONBINARY-0001 - Split exact and versioned binary reads](serializationbinary-0001-split-exact-and-versioned-binary-reads.md)
- [SERIALIZATIONBINARY-0002 - Optimize packed types as raw native-layout bytes](serializationbinary-0002-optimize-packed-types-as-raw-native-layout-bytes.md)
- [SERIALIZATIONBINARY-0003 - Allow schema-prefixed payloads for best-effort compatibility](serializationbinary-0003-allow-schema-prefixed-payloads-for-best-effort-compatibility.md)
