# Reflection Architecture

## Purpose

Reflection describes C++ types at compile time so other libraries, especially serialization, can walk object structure without RTTI, exceptions, STL, hidden allocation, or intrusive base classes. Future changes must treat Reflection as a schema compiler and type-description layer, not as a runtime object model.

## Architectural Shape

Reflection is header-only. Users describe structs externally by specializing `SC::Reflection::Reflect<T>` or by using the `SC_REFLECT_STRUCT_*` macros. The schema compiler turns those visitors into compact flat `TypeInfo` arrays, type-name arrays, and optional builder-specific tables.

Reflection metadata must stay constexpr-friendly. The core shape is explicit field visits, primitive and container category specializations, recursive packed-type detection, and link-indexed flat schemas. Avoid adding runtime registries, heap-owned trees, or global mutable metadata.

## Boundaries

Reflection owns type categories, struct member descriptions, schema compilation, packed-layout metadata, and minimal extension points for reflectable containers. It does not own serialization formats, storage allocation, dynamic JSON documents, runtime type discovery, or automatic C++ reflection experiments.

Container and string support belongs in adapter libraries such as ContainersReflection. Reflection itself should not depend on Containers, Memory, Strings, SerializationBinary, or SerializationText.

## Similarities With Other Libraries

Reflection follows the project-wide single-file and independent-consumption model. It uses Common fragments for primitive types, type traits, spans, compiler macros, and offsets. Like SerializationBinary and SerializationText, it shifts allocation and storage decisions to callers or consuming libraries.

## Differences From Other Libraries

Reflection is more template-heavy than most Sane C++ libraries because its purpose is compile-time schema generation. That is an exception to the usual preference for moving code into `.cpp` files, but the template surface should remain readable and narrowly tied to schema compilation.

Unlike serializers, Reflection must not interpret data bytes or produce serialized formats. Unlike ContainersReflection, it must not adapt concrete container implementations beyond the minimal generic hooks in its own namespace.

## Inspirations

No named external reflection library is cited in local sources. The evidenced inspiration is the project's own principles: explicit memory ownership, fast integration through single-file libraries, simple readable code, and compile-time checks where they reduce runtime machinery.

## Anti-Inspirations

Reflection is explicitly not an RTTI system, not a C++ exception-based visitor framework, not a heap-backed metadata registry, and not an intrusive base-class hierarchy. Inference: it is also intentionally not modeled after broad serialization frameworks that infer field names or offsets through compiler-specific magic, because the documentation prefers explicit fields for portability and compile-time cost.

## Architectural Choices

New reflected fields must preserve the explicit member tag, pointer-to-member, field name, and offset inputs unless a later ADR supersedes that contract. Binary compatibility uses tags; text compatibility uses names.

Schemas must remain flat and compact. Complex types should link to definitions by index, and packed metadata should continue to describe whether raw byte serialization is allowed. If schema limits need to expand, update the schema representation deliberately and record the compatibility cost.

Any new category or adapter hook must be justified by a serializer or documented consumer. Do not add generic runtime reflection features just because they are common in other ecosystems.

## Explicitly Excluded Targets

- Runtime reflection or dynamic type registration.
- Hidden allocation for schema storage.
- Public dependencies on Containers, Memory, Strings, or Serialization libraries.
- Reflection features that require RTTI, exceptions, STL, or C++20-only field-name extraction.
- Dynamic JSON document modeling; that belongs outside this scope.

## Sources

- [Reflection documentation](../../Documentation/Libraries/Reflection.md)
- [Reflection public API](../../Libraries/Reflection/Reflection.h)
- [Reflection schema compiler](../../Libraries/Reflection/ReflectionSchemaCompiler.h)
- [Reflection tests](../../Tests/Libraries/Reflection/ReflectionTest.cpp)
- [Project principles](../../Documentation/Pages/Principles.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0005 - Avoid STL, exceptions, and RTTI in library code](../Global/sc-0005-avoid-stl-exceptions-and-rtti-in-library-code.md)
- [REFLECTION-0001 - Use External constexpr Reflect&lt;T&gt; Specializations](reflection-0001-use-external-constexpr-reflect-t-specializations.md)
- [REFLECTION-0002 - Store Member Tags, Names, Pointers, and Offsets in Schema Input](reflection-0002-store-member-tags-names-pointers-and-offsets-in-schema-input.md)
- [REFLECTION-0003 - Represent Schemas as Flat TypeInfo Arrays with Packed-Type Metadata](reflection-0003-represent-schemas-as-flat-typeinfo-arrays-with-packed-type-metadata.md)

## Decision Log

- [REFLECTION-0001 - Use external constexpr Reflect<T> specializations](reflection-0001-use-external-constexpr-reflect-t-specializations.md)
- [REFLECTION-0002 - Store member tags, names, pointers, and offsets in schema input](reflection-0002-store-member-tags-names-pointers-and-offsets-in-schema-input.md)
- [REFLECTION-0003 - Represent schemas as flat TypeInfo arrays with packed-type metadata](reflection-0003-represent-schemas-as-flat-typeinfo-arrays-with-packed-type-metadata.md)
