@page library_containers Containers

@brief 🟨 Generic containers (SC::Vector, SC::SmallVector, SC::Array etc.)

[TOC]

[SaneCppContainers.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppContainers.h) is a library holding some commonly used templated data structures.

While all libraries are designed to let you use your favorite externally provided `string` / `vector` classes, there is also an choice of basic containers (mainly `Vector<T>` incarnations), with support for inline buffer and custom scoped allocators provided by [Memory](@ref library_memory) library.

@note See `Tests/InteropSTL/*.cpp` for an example of externally provided Container classes.

# Dependencies
- Dependencies: [Memory](@ref library_memory)
- All dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory)

![Dependency Graph](Containers.svg)


# Features
| Class                             | Description                               |
|:----------------------------------|:------------------------------------------|
| SC::Vector                        | @copybrief SC::Vector                     |
| SC::Array                         | @copybrief SC::Array                      |
| SC::SmallVector                   | @copybrief SC::SmallVector                |
| SC::VectorMap                     | @copybrief SC::VectorMap                  |
| SC::VectorSet                     | @copybrief SC::VectorSet                  |
| SC::ArenaMap                      | @copybrief SC::ArenaMap                   |

# Status
🟨 MVP  
All classes defined in the library should be reasonably stable and safe to use.  

# Description

Generic data structures are a fundamental building blocks for almost every application.  
These are some of commonly used ones for common tasks, and the library will grow adding what's needed.

SC::Vector is the king of all generic containers for this library, being in many case the main backend storage for other containers.

SC::Array mimics all methods of SC::Vector but it's guaranteed never to allocate on heap.  
All methods are designed to fail with a `[[nodiscard]]` return value when the container is full.

SC::SmallVector is the middle ground between SC::Array and SC::Vector.  
It's a vector with inline storage for `N` elements, deriving from SC::Vector and it's designed to be passed everywhere a reference to SC::Vector is needed. This allows the caller to get rid of temporary heap allocations if an estimate of the space required is already known or if it's possible providing a reasonable default.  
If this estimation is wrong, heap allocation will happen.

# Blog

Some relevant blog posts are:

- [February 2025 Update](https://pagghiu.github.io/site/blog/2025-02-28-SaneCppLibrariesUpdate.html)

## Vector

@copydoc SC::Vector

## Array

@copydoc SC::Array

## SmallVector

@copydoc SC::SmallVector

## VectorMap

@copydoc SC::VectorMap

## VectorSet

@copydoc SC::VectorSet

## ArenaMap

@copydoc SC::ArenaMap

# Details
- SC::Segment is the class representing a variable and contiguous slice of bytes or objects backing both SC::Vector, SC::SmallVector, SC::Array, SC::Buffer and SC::SmallBuffer.  
- Memory layout of a segment is a SC::SegmentHeader holding size and capacity of the segment followed by the actual elements. 
- SC::SegmentHeader is aligned to `uint64_t`.

# Roadmap

🟩 Usable Features:
- Add option to let user disable heap allocations in SC::SmallVector
- Explicit control on Segment / Vector allocators
- `HashMap<T>`
- `Map<K, V>`

🟦 Complete Features:
- More specific data structures

💡 Unplanned Features:
- None

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 868			| 417		| 1285	|
| Sources   | 0			| 0		| 0	|
| Sum       | 868			| 417		| 1285	|
