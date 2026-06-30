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
| SC::Span                  | Non-owning contiguous view over caller-provided memory
| SC::StringSpan            | Non-owning string view with explicit encoding
| SC::StringPath            | Non-owning path string view
| SC::Result                | Small success/error result type for APIs without exceptions
| SC::Function              | Fixed-storage callable wrapper
| SC::Deferred              | Scope-exit helper for explicit cleanup
| SC::OpaqueObject          | Fixed-size opaque storage for static PIMPL
| SC::UniqueHandle          | Move-only RAII wrapper for handles

## Macros
- [Compiler Macros](@ref group_foundation_compiler_macros): @copybrief group_foundation_compiler_macros

## Type Traits
- Type traits: small compile-time helpers used by Foundation and embedded Common fragments

## Utilities
| Class                     | Description
|:--------------------------|:--------------------------------|
| SC::Assert                | Foundation-owned assertion provider
| SC::AlignedStorage        | Fixed-size aligned storage for hiding implementation details

# Status
🟩 Usable  
The library is very simple it it has what is needed so far by the other libraries.

# Description
There is an hard rule in the library [Principles](@ref page_principles) not to include system and compiler headers in public headers.  
Foundation provides all primitive types to be used in headers and classes like SC::UniqueHandle, SC::OpaqueObject, SC::AlignedStorage to encourage static PIMPL in order to hide platform specific implementation details everywhere.

## Function
Fixed-storage callable wrapper used when APIs need callbacks without dynamic allocations.

## Deferred
Scope-exit helper used to run explicit cleanup code when leaving a scope.

## OpaqueObject
Fixed-size opaque storage used to hide private implementation details in public headers.

## UniqueHandle
Move-only RAII wrapper used to close or release handles deterministically.

# Blog

Some relevant blog posts are:

- [January 2025 Update](https://pagghiu.github.io/site/blog/2025-01-31-SaneCppLibrariesUpdate.html)
- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)
- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)
- [May 2026 Update](https://pagghiu.github.io/site/blog/2026-05-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟦 Complete Features:
- Things will be added as needed

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Foundation`.
Single File counts
`SaneCppFoundation.h`.
Standalone counts `SaneCppFoundationStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 27		| 64		| 91	|
| Single File | 1597		| 216		| 1813	|
| Standalone  | 1597		| 216		| 1813	|
