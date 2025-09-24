@page library_serialization_binary Serialization Binary

@brief 🟨 Serialize to and from a binary format using [Reflection](@ref library_reflection)

[TOC]

@copydetails group_serialization_binary

# Dependencies
- Direct dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Reflection](@ref library_reflection)
- All dependencies: [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Reflection](@ref library_reflection)

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 572			| 290		| 862	|
| Sources   | 0			| 0		| 0	|
| Sum       | 572			| 290		| 862	|

# Features
- No heap allocations
- Serialize primitive types (Little Endian)
- Serialize Vector-like types (including SC::Vector, SC::Array, SC::String)
- Serialize C-Array-like types (`T[N]`)
- Serialize Structs composed of above types or other structs
- Optimization for _Packed_ types
- Optimized fast code path when deserializing data generated with same schema version
- Automatic *versioned* deserialization (without losing data) generated with a different schema version for following events:
    - Dropping fields
    - Adding new fields
    - Dropping excess array members
    - Moving fields in structs
    - Integer to / from float conversions

# Status

🟨 MVP  
Under described limitations, the library should be usable but more testing is needed and also supporting all of the relevant additional container data-types.

# Description

- Writing data can be done with the SC::SerializationBinary::write.
- Reading data without changing the reflected data structures, can be done with the SC::SerializationBinary::loadExact.
- Reading data generated from a different (past or future) version of the data structure, can be done with the SC::SerializationBinary::loadVersioned and uses [Reflection](@ref library_reflection) *Flat Schema*.

## SerializationBinary::write
@copydetails SC::SerializationBinary::write

## SerializationBinary::loadExact
@copydetails SC::SerializationBinary::loadExact

## SerializationBinary::loadVersioned
@copydetails SC::SerializationBinary::loadVersioned

@note The versioned serializer is greatly simplified in conjunction with [Reflection](@ref library_reflection) sorting `Packed` structs by `offsetInBytes`.

## SerializationBinaryOptions
@copydoc SC::SerializationBinaryOptions

| Option                                                                                            | Description                                                               |
|:--------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------|
| [allowFloatToIntTruncation](@ref SC::SerializationBinaryOptions::allowFloatToIntTruncation)       | @copybrief SC::SerializationBinaryOptions::allowFloatToIntTruncation      |
| [allowDropExcessArrayItems](@ref SC::SerializationBinaryOptions::allowDropExcessArrayItems)       | @copybrief SC::SerializationBinaryOptions::allowDropExcessArrayItems      |
| [allowDropExcessStructMembers](@ref SC::SerializationBinaryOptions::allowDropExcessStructMembers) | @copybrief SC::SerializationBinaryOptions::allowDropExcessStructMembers   |

## Binary Format
The binary format is defined as follows:

- Primitive types (`int`, `float` etc) are `Packed` by definition and get dumped to binary stream as is (with their native endian-ness)
- `struct` are serialized:
    - If `Packed` == `True`: In a single `memcpy`-dump (as if sorted by `offsetInBytes`)
    - If `Packed` == `False`: Serializing each field sorted by their visit order (and not the `memberTag`)
- `T[N]` arrays (fixed number of elements) are serialized:
    - If item type `Packed` == `True`: In a single `memcpy`-dump
    - If item type `Packed` == `False`: Serializing each array item in sequence
- `Vector<T>` (variable number of elements) are serialized:
    - Serialize number of elements as an `uint64_t`
    - If item type `Packed` == `True`: In a single `memcpy`-dump
    - If item type `Packed` == `False`: Serializing each vector item in sequence

## Packed types (optimization)
If a `struct`, `T[N]` array or content of `Vector<T>` is made of a recursively `Packed` type (i.e. no padding bytes at any level inside the given type) it will be serialized in a single operation.  
This can really condense a large number of operations in a single one on types obeying to the `Packed` property.

@note It's possible to `static_assert` the `Packed` property of a type, if one wants to be sure not to accidentally introduce padding bytes in serialized types.

# Blog

Some relevant blog posts are:

- [July 2024 Update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html)

# Alternative implementation
The binary serializer has an additional parallel implementation, see [SerializationBinaryTypeErased](@ref library_serialization_binary_type_erased) in Libraries Extra.  
This is just an experiment to check if with some more runtime-code and less compile-time-code we can further bring down compile times, but we still need to build a proper benchmark for it.

# Roadmap
The binary serializer is not a streaming one, so loading a large data structure will at some point need double of the required space in memory.  
This can be solved implementing streaming binary serializer or experimenting with memory mapped files to support really large binary data structures.

🟩 Usable  
- SC::ArenaMap serialization
- SC::SmallVector serialization
- SC::SmallString serialization

🟦 Complete Features:
- Streaming serializer

💡 Unplanned Features:
- None so far
