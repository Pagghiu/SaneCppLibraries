@page library_foundation Foundation

@brief 🟩 Primitive types, asserts, limits, Function, Span, Result, Tagged Union

[TOC]

Foundation library provides many fundamental type definitions and types widely used by other libraries.  
As this is included and needed by almost every other library, it tries to keep bloat to the bare minimum.  
Detailed documentation is in the @ref group_foundation topic.

# Features

## Classes
| Class                     | Description
|:--------------------------|:--------------------------------|
| SC::Span                  | @copybrief SC::Span
| SC::SpanString            | @copybrief SC::SpanString
| SC::SpanStringView        | @copybrief SC::SpanStringView
| SC::Result                | @copybrief SC::Result
| SC::Function              | @copybrief SC::Function
| SC::Deferred              | @copybrief SC::Deferred
| SC::OpaqueObject          | @copybrief SC::OpaqueObject
| SC::TaggedUnion           | @copybrief SC::TaggedUnion
| SC::UniqueHandle          | @copybrief SC::UniqueHandle

## Macros
- [Compiler Macros](@ref group_foundation_compiler_macros): @copybrief group_foundation_compiler_macros

## Type Traits
- [Type Traits](@ref group_foundation_type_traits): @copybrief group_foundation_type_traits

## Utilities
| Class                     | Description
|:--------------------------|:--------------------------------|
| SC::Assert                | @copybrief SC::Assert
| SC::AlignedStorage        | @copybrief SC::AlignedStorage
| SC::MaxValue              | @copybrief SC::MaxValue
| SC::Memory                | @copybrief SC::Memory

# Status
🟩 Usable  
The library is very simple it it has what is needed so far by the other libraries.

# Description
There is an hard rule in the library [Principles](@ref page_principles) not to include system and compiler headers in public headers.  
Foundation provides all primitive types to be used in headers and classes like SC::UniqueHandle, SC::OpaqueObject, SC::AlignedStorage to encourage static PIMPL in order to hide platform specific implementation details everywhere.

## Function
@copydoc SC::Function

## Deferred
@copydoc SC::Deferred

## TaggedUnion
@copydoc SC::TaggedUnion

## OpaqueObject
@copydoc SC::OpaqueObject

## UniqueHandle
@copydoc SC::UniqueHandle

# Blog

Some relevant blog posts are:

- [January 2022 Update](https://pagghiu.github.io/site/blog/2025-01-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟦 Complete Features:
- Things will be added as needed

💡 Unplanned Features:  

- SharedPtr
- UniquePtr

@note In [Principles](@ref page_principles) there is a rule that discourages allocations of large number of tiny objects and also creating systems with unclear or shared memory ownership.
For this reason this library is missing Smart Pointers.

