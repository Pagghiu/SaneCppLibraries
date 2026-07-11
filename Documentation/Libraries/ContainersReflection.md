@page library_containers_reflection Containers Reflection

@brief 🟨 Opt-in reflection and serialization adapters for SC containers and owning memory types.

[TOC]

[SaneCppContainersReflection.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppContainersReflection.h) connects the generic [Reflection](@ref library_reflection), [Serialization Binary](@ref library_serialization_binary), and [Serialization Text](@ref library_serialization_text) machinery to SC's owning containers. Include its adapters where a reflected object graph contains types such as `SC::Vector`, `SC::Array`, `SC::VectorMap`, `SC::Buffer`, or `SC::String`.

# Dependencies
- Dependencies: [Containers](@ref library_containers), [Reflection](@ref library_reflection)
- All dependencies: [Containers](@ref library_containers), [Memory](@ref library_memory), [Reflection](@ref library_reflection)

![Dependency Graph](ContainersReflection.svg)


# What Containers Reflection Is For

Reflection can describe structures and primitive values without knowing about growable storage. Serialization can walk
that description without choosing which container library an application uses. Containers Reflection supplies the
missing, opt-in knowledge for SC's own containers.

This separation matters because SC libraries do not acquire dependencies merely for convenience. Reflection and the
serialization libraries remain usable without [Containers](@ref library_containers) or [Memory](@ref library_memory);
an application pays for these adapters only when its reflected types need them.

Use this library when:

- a reflected structure contains an `SC::Array`, `SC::Vector`, `SC::VectorMap`, `SC::Buffer`, or `SC::String`;
- binary or text serialization must resize an owning SC container while reading;
- you want the standard SC representation of these types rather than writing application-specific reflection traits.

It is usually not something an application calls directly. Its public surface consists of template specializations
selected by the compiler after the appropriate header is included.

# Choose The Adapter Header

The library keeps container and memory adapters separate, and keeps reflection-only use separate from serialization:

| Include | Adds support for |
|---------|------------------|
| `ContainersReflection.h` | Reflecting `SC::Array`, `SC::Vector`, and `SC::VectorMap` |
| `MemoryReflection.h` | Reflecting `SC::Buffer`, `SC::String`, and `SC::StringEncoding` |
| `ContainersSerialization.h` | Binary and text serialization of `SC::Array` and `SC::Vector` |
| `MemorySerialization.h` | Binary serialization of `SC::Buffer` and text serialization of `SC::String` |

The serialization headers include their corresponding reflection headers. Include only the layer the translation unit
uses; there is no initialization function or object to retain.

# Mental Model

Reflection treats `SC::Array` and `SC::Vector` as vector-like values: it can obtain their size and data, identify their
element type, and ask them to resize. A fixed-capacity `SC::Array<T, N>` clamps adapter resize requests to `N`; a
`SC::Vector<T>` can grow as far as its allocator permits. `SC::Buffer` follows the same vector-like model for bytes.
That clamp is a capacity boundary, not a general promise that oversized exact-format input is accepted: input sizes
still need to match the guarantees of the serializer being used.

`SC::VectorMap` is different. Its reflection describes the map through its `items` storage, preserving the container's
actual representation instead of inventing a separate wire-level map abstraction. `SC::String` is reflected through
its encoding and data members, while text serialization has direct string handling.

The adapters contain no per-object state and perform growth only through the adapted object. Deserializing a vector,
buffer, or string may therefore allocate through that object's configured allocator. A failed resize makes the
surrounding serialization operation fail; existing contents should not be treated as a transactionally preserved
value unless the selected serializer explicitly provides that guarantee.

# Nested Containers Need No Per-Field Adapter

Once `ContainersReflection.h` is included, ordinary reflection declarations can contain nested vectors. The following
compiled test reflects both `Vector<int>` and `Vector<SimpleStructure>` fields:

@snippet Tests/Libraries/Reflection/ReflectionTest.cpp reflectionSnippet2

The structure still declares its own fields, but it does not explain how a vector is sized, traversed, or resized. That
knowledge comes from this library's `Reflect` and `ExtendedTypeInfo` specializations.

# The Same Types Flow Into Serialization

The serialization adapters reuse that model. This compiled JSON test combines a built-in fixed array, `SC::String`,
and `SC::Vector<SC::String>` in one reflected object:

@snippet Tests/Libraries/SerializationText/SerializationJsonTest.cpp serializationJsonSnippet1

Including `ContainersSerialization.h` and `MemorySerialization.h` makes those fields available to the generic text and
binary serializers. The adapters do not choose an allocator, hide capacity failures, or change the lifetime rules of
the objects being loaded.

# Boundaries And Tradeoffs

- Support is opt-in and compile-time. Omitting an adapter header produces a missing specialization at compilation,
  rather than a runtime registration failure.
- `SC::VectorMap` gains reflection here, but no direct serialization specialization. Its default reflected shape is a
  structure with an `items` field; it is not serialized as a format-specific map or JSON object.
- The standard adapters describe SC's concrete representation. If a stable external format must differ from that
  representation, define an explicit schema or application-level conversion instead of relying on the default shape.
- Owning values may allocate while loading. Borrowed views and spans have different lifetime and input-buffer rules and
  are handled by their respective serialization facilities, not turned into owning containers here.
- This library is intentionally coupled to both sides of the seam. Adding support for another container belongs here
  when doing so would otherwise force Reflection or Serialization to depend on that container library.

# Status

🟩 Usable

# Where To Go Next

- [Reflection](@ref library_reflection) explains how to declare fields and compile schemas.
- [Serialization Binary](@ref library_serialization_binary) covers exact and versioned binary formats.
- [Serialization Text](@ref library_serialization_text) covers JSON and text-oriented loading.
- [Containers](@ref library_containers) and [Memory](@ref library_memory) document allocation, ownership, and capacity
  behavior of the adapted types.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/ContainersReflection`.
Single File counts
`SaneCppContainersReflection.h`.
Standalone counts `SaneCppContainersReflectionStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 231		| 0		| 231	|
| Single File | 251		| 0		| 251	|
| Standalone  | 4269		| 1340		| 5609	|
