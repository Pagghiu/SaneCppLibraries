@page library_memory Memory

@brief 🟩 Heap Allocation, Custom allocators, Virtual Memory, Buffer, Segment

[TOC]

[SaneCppMemory.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppMemory.h) is library tracking and limiting runtime / dynamic allocations through the use of custom allocators.

All libraries are designed to let you use your favorite externally provided `string` / `vector` classes BUT if you've been sold on using also the [Containers](@ref library_containers) library then here you will find scoped `allocator` / `arena` infrastructure used by them.

@note See `Tests/InteropSTL/*.cpp` for an example of externally provided Container classes.

The "owned" String class is here and not in [Strings](@ref library_strings) library because it needs allocator support.

@note If a library doesn't directly or indirectly depend on the Memory library, you can assume that it will not do any runtime / dynamic allocation.

# Dependencies
- Dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

![Dependency Graph](Memory.svg)


# Features

## Classes
| Class                     | Description
|:--------------------------|:--------------------------------|
| SC::Buffer                | @copybrief SC::Buffer
| SC::Memory                | @copybrief SC::Memory
| SC::VirtualMemory         | @copybrief SC::VirtualMemory
| SC::Globals               | @copybrief SC::Globals
| SC::String                | @copybrief SC::String
| SC::SmallString           | @copybrief SC::SmallString

# Status
🟩 Usable  
The library is solid. The Buffer implementation has been evolved and fine tuned to be minimal but effective.

# Description
Memory library helps tracking and limit runtime / dynamic allocations through the use of custom allocators.
A classic dynamically expandable binary buffer SC::Buffer is provided and it's largely shared to form the more _object model oriented_ SC::Vector class from [Containers](@ref library_containers) Library.

All allocations throughout all downstream dependant libraries are centrally tracked by the SC::Globals class, that also allows re-defining custom thread-local allocators.

Such allocators can be just fixed buffers, regular heap memory or reserved SC::VirtualMemory using only limited amounts of Physical memory.

## Buffer
@copydoc SC::Buffer

## String
@copydoc SC::String

## SmallString
@copydoc SC::SmallString

## Memory
@copydoc SC::Memory

## VirtualMemory
@copydoc SC::VirtualMemory

## Globals
@copydoc SC::Globals

# Blog

These blogs have been written before the split from Foundation into the Memory library:

- [February 2025 Update](https://pagghiu.github.io/site/blog/2025-02-28-SaneCppLibrariesUpdate.html)
- [March 2025 Update](https://pagghiu.github.io/site/blog/2025-03-31-SaneCppLibrariesUpdate.html)
- [April 2025 Update](https://pagghiu.github.io/site/blog/2025-04-30-SaneCppLibrariesUpdate.html)

These blog posts have been written after the split from foundation:

- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)

# Roadmap

🟦 Complete Features:
- Things will be added as needed

💡 Unplanned Features:  

- SharedPtr
- UniquePtr

@note In [Principles](@ref page_principles) there is a rule that discourages allocations of large number of tiny objects and also creating systems with unclear or shared memory ownership.
For this reason this library is missing Smart Pointers.

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 453			| 406		| 859	|
| Sources   | 1056			| 223		| 1279	|
| Sum       | 1509			| 629		| 2138	|
