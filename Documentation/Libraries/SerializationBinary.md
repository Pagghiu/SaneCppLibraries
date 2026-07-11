@page library_serialization_binary Serialization Binary

@brief 🟨 Reflection-driven binary persistence with exact and schema-aware reads

[TOC]

[SaneCppSerializationBinary.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerializationBinary.h)
turns a reflected C++ object graph into a compact sequence of bytes and reconstructs it later. It is aimed at local
state, caches, and other controlled persistence where the producer and consumer share the same basic C++ data model.

This is deliberately not a general-purpose wire format. Primitive values and packed objects are copied in their native
representation, and an embedded schema is itself a binary array of Reflection metadata. Endianness, primitive sizes,
compiler ABI, and schema representation therefore matter. Choose a format with an explicit portable encoding when data
must cross unrelated architectures, toolchains, or languages.

# Dependencies
- Dependencies: [Reflection](@ref library_reflection)
- All dependencies: [Reflection](@ref library_reflection)

![Dependency Graph](SerializationBinary.svg)


# The decision this library makes

Serialization Binary separates two concerns that are often conflated:

- `write` emits only object data. `loadExact` reads it with the current reflected layout and is the direct, fast path.
- `loadVersioned` receives the source [Reflection](@ref library_reflection) flat schema separately and maps fields by
  stable `memberTag` values into the destination type.
- `writeWithSchema` prepends that schema, while `loadVersionedWithSchema` selects `loadExact` when the embedded schema
  equals the current schema and otherwise falls back to the versioned reader.

The useful mental model is therefore **bytes plus a schema identity**, not a self-describing archive in the usual sense.
If the schema identity is already stored in a file header, database record, or protocol envelope, the separate
`write`/`loadExact`/`loadVersioned` APIs avoid repeating it. For a small, self-contained local state file,
`writeWithSchema` and `loadVersionedWithSchema` are the more convenient pair.

# A representative reflected model

Types are described with Reflection and can be nested from primitives, fixed arrays, reflected structs, and supported
container adapters. This definition is compiled as part of `SerializationBinaryTest`:

@snippet Tests/Libraries/SerializationBinary/SerializationSuiteTest.h serializationBinaryExactSnippet1

For an unchanged `TopLevelStruct`, pass a caller-owned appendable buffer to
`SC::SerializationBinary::write`, then pass its `Span<const char>` to `SC::SerializationBinary::loadExact`. Both calls
return `bool`; a failed append, a short input, an unsupported resize, or trailing bytes makes the operation fail.

For durable local state, the repository's Serialization example uses `writeWithSchema` to fill an `SC::Buffer`, writes
that buffer with [File](@ref library_file), reads the file back into another buffer, and calls
`loadVersionedWithSchema`. This keeps file I/O out of the serializer and makes buffer ownership explicit.

# What versioned loading can and cannot absorb

The versioned reader uses member tags rather than member declaration order. With stable, unique tags it can match moved
fields, ignore source fields absent from the destination, leave newly added destination fields at their existing
values, shorten arrays, and perform supported primitive conversions. The test suite exercises a source and destination
whose fields are reordered and removed:

@snippet Tests/Libraries/SerializationBinary/SerializationSuiteTest.h serializationBinaryVersionedSnippet1

`SerializationBinaryOptions` controls the intentionally lossy cases. Its defaults currently allow float-to-integer
truncation, dropping excess array items, dropping unmatched struct members, and conversions between `bool` and other
primitive types. Tighten those flags when silent loss would be worse than rejecting old data.

Versioned loading is best-effort migration, not arbitrary data evolution. Changing the meaning of a tag, reusing a
retired tag, or changing to an unsupported shape still needs an application-level migration. It is also the caller's
job to retain or embed the exact source schema; compiling only today's schema cannot describe yesterday's byte stream.

# Storage, allocation, and lifetime

The serializer itself does not own an archive and does not allocate hidden working storage:

- Writers append to the buffer supplied by the caller. A fixed-capacity buffer gives a bounded, allocation-free write;
  a growable `Buffer` or `Vector` may allocate according to that container's policy.
- Readers borrow the input `Span<const char>` for the duration of the call and copy values into the destination. The
  byte span and a separately supplied schema span only need to remain valid until the call returns.
- Deserializing a variable-length container resizes the destination. Whether that allocates, and whether it can fail for
  insufficient capacity, is determined by the container and its Reflection adapter.
- The entire encoded value must currently be present in memory. There is no incremental reader or writer, so file I/O
  commonly requires both the serialized buffer and destination object to coexist.

To serialize `SC::Vector`, `SC::Array`, `SC::String`, or `SC::Buffer`, also include the appropriate headers from
[Serialization Adapters](@ref library_containers_reflection). Those adapters live outside this library so
Serialization Binary depends only on Reflection; support for a container is not implied merely because it is an SC
type.

# Binary layout and the packed fast path

The format is intentionally close to memory:

- Primitive values are copied using their native representation and endianness.
- A non-packed struct writes its reflected members in visit order; member tags are metadata for versioned matching, not
  field positions in the byte stream.
- A fixed C array writes its elements in sequence.
- A vector-like value writes its payload size in bytes as a `uint64_t`, followed by its elements. This is a byte count,
  not an element count.
- A recursively `Packed` struct, array, or container payload can be transferred in one operation. Otherwise the walker
  descends into its fields or elements.

Here `Packed` is Reflection's stronger property: the complete value can be copied without padding or omitted state. It
is not the same as applying a compiler packing pragma. A `static_assert` on Reflection's packed trait can protect code
that relies on the one-operation path, but changing packing or native representation still changes the stored bytes.

`numberOfWrites` and `numberOfReads` expose the number of byte-transfer operations and are useful for checking this
optimization in tests. They are not a stable part of the file format and should not be used as a compatibility marker.

# Fit with neighboring libraries

[Reflection](@ref library_reflection) supplies the type graph, field tags, packed analysis, and flat schemas; this
library supplies the binary traversal and compatibility logic. [Serialization Adapters](@ref
library_containers_reflection) connects container and memory types without introducing library-to-library dependencies.

[Serialization Text](@ref library_serialization_text) is the neighboring choice when inspectability and a conventional
text representation matter more than compact native bytes. Binary serialization avoids tokenization and can collapse
packed data into large copies, while text is easier to inspect, edit, diff, and exchange. Neither choice removes the
need for deliberate field tags and migration policy.

# Current limits

The library is marked MVP: the exact and versioned paths are exercised for primitives, nested structs, arrays, vectors,
strings, conversions, dropped data, and packed structs, but container coverage and compatibility testing are not yet
complete. There is no streaming mode, integrity check, authentication, compression, object identity, pointer graph, or
endianness normalization. Treat input as trusted unless the surrounding application validates its size and provenance;
lengths in untrusted archives can drive destination-container growth.

The [Serialization example](https://github.com/Pagghiu/SaneCppLibraries/tree/master/Examples/SCExample/Examples/SerializationExample)
shows binary and JSON persistence side by side. The [July 2024 update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html)
also discusses the serializer's development.

# API reference

@copydetails group_serialization_binary

@copydoc SC::SerializationBinaryOptions

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/SerializationBinary`.
Single File counts
`SaneCppSerializationBinary.h`.
Standalone counts `SaneCppSerializationBinaryStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 593		| 0		| 593	|
| Single File | 1013		| 0		| 1013	|
| Standalone  | 1943		| 0		| 1943	|
