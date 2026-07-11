@page library_reflection Reflection

@brief 🟩 Describe C++ object structure at compile time, primarily for serialization.

[TOC]

[SaneCppReflection.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppReflection.h) lets an application describe the fields of its own C++ structures without adding metadata to those structures. The description is `constexpr`, allocation-free, and deliberately small: primitive values, C arrays, structures, and adapter-defined vector-like types.

Reflection is infrastructure rather than a general-purpose introspection system. Its main consumers are [Serialization Binary](@ref library_serialization_binary) and [Serialization Text](@ref library_serialization_text). It does not discover arbitrary C++ members, access fields by a dynamic string API, perform I/O, or manage object storage.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Reflection.svg)


# When Reflection Fits

Use Reflection when the set of serializable fields is known at compile time and you are willing to register those fields explicitly. It is a good fit for application state, messages, settings, or file-format structures that need one shared description for binary and textual serialization.

It is less suitable when you need runtime registration, arbitrary graph traversal, polymorphic object construction, or transparent support for pointers and references. Those types have no built-in reflection model. Reflection also does not preserve C++ invariants during deserialization by itself; that policy belongs to the serializer and the reflected type design.

The library has no dependency on SC containers. Primitive types, C arrays, and user structures work directly. If an object graph contains `SC::Vector`, `SC::Array`, `SC::VectorMap`, `SC::Buffer`, or `SC::String`, include the opt-in [Containers Reflection](@ref library_containers_reflection) adapters. This keeps the base library independent, but means container support is not automatic merely because the container header is visible.

# The Description You Write

A reflected structure specializes `SC::Reflection::Reflect<T>` outside the structure itself. Its `visit` function presents each selected member as four pieces of information:

- a numeric member tag;
- a pointer-to-member;
- a textual field name;
- the byte offset within the containing structure.

The member pointer is how serializers access the value without casting from a byte offset. Text serializers use the name, while versioned binary serialization uses the numeric tag. Keeping both explicit lets a C++ member be renamed without necessarily breaking the binary format, or lets a textual name change independently when a format intentionally changes. Tags therefore belong to the persisted format: do not renumber them casually, and do not reuse a removed tag for a different meaning.

The `SC_REFLECT_STRUCT_VISIT`, `SC_REFLECT_STRUCT_FIELD`, and `SC_REFLECT_STRUCT_LEAVE` macros are the usual spelling. This test-backed example shows composition and SC container adapters in the same object graph:

@snippet Tests/Libraries/Reflection/ReflectionTest.cpp reflectionSnippet2

The order written is normally the visit order used by exact serializers. The field name is produced from the member token by the macro. For unusual mappings, the underlying explicit `Reflect<T>` specialization remains available; the complete form is exercised here:

@snippet Tests/Libraries/Reflection/ReflectionTest.cpp reflectionSnippet1

# Two Ways Consumers Use It

The member visitor is the simplest mental model. A consumer calls `Reflect<T>::visit(visitor)`, and the visitor receives each registered pointer-to-member plus its metadata. Exact binary and text serializers use this path directly, recursing into the member's reflected type.

For versioning and type-level inspection, `SC::Reflection::Schema::compile<T>()` converts the same description into a flat compile-time schema. Each entry is an eight-byte `SC::Reflection::TypeInfo`. A structure entry is followed by entries for its fields; a non-primitive field links to the later entry that describes that structure or array. Repeated complex types share their linked description rather than expanding forever.

The result contains parallel fixed-size arrays of type information and type names, trimmed at compile time to the entries actually used. Schema compilation performs no heap allocation and creates no runtime registry. Its default compile-time working limits are 20 distinct complex links and 100 total type entries; callers with a larger model can override both template arguments. The stored representation itself uses an eight-bit link index and child count and 16-bit sizes and offsets. Although the member-tag field occupies 16 bits, the current registration and schema-building interfaces accept an eight-bit tag. This is intentionally a compact schema rather than an unbounded metadata format.

# Packing Is a Serialization Property

Reflection computes `ExtendedTypeInfo<T>::IsPacked` recursively. A structure is considered packed only when every reflected member is packed and the sum of their sizes equals `sizeof(T)`. C arrays inherit the packing state of their element type. This detects padding between reflected fields and padding introduced by nested types.

The name can be misleading: Reflection does not apply a compiler packing attribute and does not change layout. It only reports that the reflected bytes already form a gap-free span. [Serialization Binary](@ref library_serialization_binary) can use that fact to copy an exact-format value in one operation; for a packed structure it may therefore write or restore member bytes without invoking per-member construction logic. A type whose invariants require setters, validation, or custom construction should not rely on that fast path merely because its physical layout happens to be packed.

For packed structures, the compiled flat schema sorts immediate member entries by byte offset. Non-packed structures retain registration order. This is an implementation contract used by binary serialization, not a reason to depend on schema order in application code.

# Allocation, Ownership, and Lifetime

Reflection owns no object data and allocates no memory. A `Reflect<T>` specialization contains compile-time functions; a compiled schema is a `constexpr` value whose storage belongs to the caller or program image. Type and field names are non-owning views into compiler-generated names or string literals and are expected to have static lifetime.

The library also does not allocate while visiting an object. Allocation can still occur in a higher-level operation: for example, a versioned deserializer may need to resize an owning vector or string through [Containers Reflection](@ref library_containers_reflection). That allocation policy and any failure reporting come from the container adapter and serializer, not from Reflection.

Offsets and sizes are narrowed into the compact schema representation. Very large structures, members beyond the representable offset, or very large schemas should be treated as outside the intended design rather than assumed to degrade gracefully. `Schema::compile` reports exhausted compile-time capacity as a compile error, but narrowing a size or offset is not a general runtime bounds-checking facility.

# Relationship to Neighboring Libraries

- [Containers Reflection](@ref library_containers_reflection) adds reflection and resize/data traits for SC owning containers. Use it only when those types occur in the reflected graph.
- [Serialization Binary](@ref library_serialization_binary) consumes member tags, layout information, packing state, and compiled schemas for exact or version-aware binary formats.
- [Serialization Text](@ref library_serialization_text) consumes field names and member visitors for JSON and other textual traversal.
- [Containers](@ref library_containers) and [Memory](@ref library_memory) provide storage types, but Reflection intentionally does not depend on them.

That separation is the main tradeoff. The base abstraction remains dependency-free and allocation-free, while applications must opt into adapters for every non-primitive family they want serializers to understand. Custom vector-like types are possible, but require more than a name: they need a `Reflect` category/build description, `ExtendedTypeInfo` access and resizing operations, and serializer specializations appropriate to each format. The Serialization example demonstrates that full extension seam with `ImVector<T>`.

# Status and Practical Limits

🟩 Usable under the documented model.

The stable center is explicit reflection of value-like structures for SC serialization. Important limits to evaluate up front are:

- no built-in pointers, references, polymorphic graphs, enums, variants, or arbitrary standard-library containers;
- metadata must be registered explicitly and kept consistent with format compatibility decisions;
- the compact flat schema has bounded integer fields and compile-time capacities;
- container ownership and deserialization allocation require separate adapters;
- packing enables byte-copy optimizations whose semantics may be inappropriate for invariant-heavy types.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Reflection`.
Single File counts
`SaneCppReflection.h`.
Standalone counts `SaneCppReflectionStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 616		| 0		| 616	|
| Single File | 930		| 0		| 930	|
| Standalone  | 930		| 0		| 930	|
