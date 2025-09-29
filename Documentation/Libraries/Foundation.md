@page library_foundation Foundation

@brief 🟩 Primitive types, asserts, compiler macros, Function, Span, Result

[TOC]

[SaneCppFoundation.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFoundation.h) is a library that provides many fundamental type definitions and types widely used by other libraries.  
As this is included and needed by almost every other library, it tries to keep bloat to the bare minimum.  
Detailed documentation is in the @ref group_foundation topic.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Foundation.svg)


# Features

## Classes
| Class                     | Description
|:--------------------------|:--------------------------------|
| SC::Span                  | @copybrief SC::Span
| SC::StringSpan            | @copybrief SC::StringSpan
| SC::StringPath            | @copybrief SC::StringPath
| SC::Result                | @copybrief SC::Result
| SC::Function              | @copybrief SC::Function
| SC::Deferred              | @copybrief SC::Deferred
| SC::OpaqueObject          | @copybrief SC::OpaqueObject
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

## OpaqueObject
@copydoc SC::OpaqueObject

## UniqueHandle
@copydoc SC::UniqueHandle

# Blog

Some relevant blog posts are:

- [January 2025 Update](https://pagghiu.github.io/site/blog/2025-01-31-SaneCppLibrariesUpdate.html)
- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)
- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟦 Complete Features:
- Things will be added as needed

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 1074			| 693		| 1767	|
| Sources   | 243			| 48		| 291	|
| Sum       | 1317			| 741		| 2058	|
