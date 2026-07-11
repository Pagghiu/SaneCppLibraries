@page library_containers Containers

@brief 🟨 Generic containers (SC::Vector, SC::SmallVector, SC::Array etc.)

[TOC]

[SaneCppContainers.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppContainers.h) provides the small set of generic containers used by Sane C++ Libraries: contiguous sequences with selectable storage, simple vector-backed maps and sets, and a stable-address arena.

# Dependencies
- Dependencies: [Memory](@ref library_memory)
- All dependencies: [Memory](@ref library_memory)

![Dependency Graph](Containers.svg)


# Status
🟨 MVP  
All classes defined in the library should be reasonably stable and safe to use.  

# What Containers Is For

Most Sane C++ libraries accept views or generic interfaces and do not require applications to adopt a particular
container family. `Containers` is the built-in option for code that needs owning collections without introducing the
STL. It is deliberately small rather than a replacement for every standard container.

Use it when you need:

- a contiguous growable sequence with explicit allocation failure;
- a fixed-capacity sequence whose elements live inside the object;
- a sequence that handles the common case inline but can grow on the heap;
- a small map or set where contiguous storage is more useful than asymptotically faster lookup;
- stable object addresses and generation-checked handles inside a fixed-capacity arena.

You can still use externally provided string and vector classes at application boundaries. See `Tests/InteropSTL/*.cpp`
for examples of interoperability with STL containers.

# Choose The Storage Policy First

`Vector<T>`, `Array<T, N>`, and `SmallVector<T, N>` present the same contiguous sequence model. The important choice is
where capacity comes from and what happens when it is exhausted.

| Type | Storage | When capacity is exhausted | Appropriate when |
|:-----|:--------|:---------------------------|:-----------------|
| `Vector<T>` | Heap, through a Memory allocator | Tries to grow and reports failure | The upper bound is unknown or large |
| `Array<T, N>` | `N` elements inline | Reports failure; never falls back to the heap | Capacity is a hard part of the design |
| `SmallVector<T, N>` | `N` elements inline, then heap | Moves to heap storage and reports allocation failure | Small collections are common but not guaranteed |

The three types are not merely similar conveniences. `SmallVector<T, N>` derives from `Vector<T>`, so code can accept a
`Vector<T>&` while callers decide whether to provide inline capacity. When a small vector shrinks back within `N`, its
elements move back to inline storage. This saves many short-lived allocations, but means element addresses are not
stable across growth or shrinking.

# A Growable Contiguous Sequence

`Vector<T>` is the basic owning sequence and the storage used by several other containers. Operations that can grow or
otherwise fail return a `[[nodiscard]] bool`; propagate that result instead of assuming allocation succeeded.

@snippet Tests/Libraries/Containers/VectorTest.cpp VectorSnippet

Copy construction and assignment cannot return an error and therefore assert if their required allocation fails. For
fallible paths, prefer operations such as `assign`, `append`, `reserve`, and `push_back`, whose result can be handled.
The allocator can be selected explicitly, including thread-local allocation through the `VectorTL` and
`SmallVectorTL` aliases declared by Containers.

# Put A Hard Bound In The Type

`Array<T, N>` keeps all storage in the object. Only `size()` elements are alive; unused capacity is not a collection of
default-constructed `T` objects. Its vector-like mutating operations return `false` when they would cross the bound.

@snippet Tests/Libraries/Containers/ArrayTest.cpp ArraySnippet

This is the clearest choice for protocols, queues, or temporary workspaces whose maximum size is known and where an
unexpected heap fallback would hide a design error.

# Optimize The Common Case Inline

`SmallVector<T, N>` keeps up to `N` elements inline while remaining usable through `Vector<T>&`. Crossing the inline
capacity is allowed, so this is an allocation optimization rather than an allocation guarantee.

@snippet Tests/Libraries/Containers/SmallVectorTest.cpp SmallVectorSnippet

Choose `N` from a meaningful workload bound, not merely to make the type appear allocation-free. A wrong estimate is
safe, but it changes allocation behavior and can invalidate pointers when storage moves.

# Small Maps And Sets Stay Linear

`VectorMap` stores unsorted key-value pairs contiguously and finds keys with a linear scan. `VectorSet` uses the same
idea for unique values. They work well for small collections, iteration-heavy workloads, and cases where avoiding the
overhead and additional implementation surface of a hash or tree structure matters. They are a poor fit for large,
lookup-heavy collections.

Both adapters accept a custom backing container. The default is `Vector`, while an `Array` or `SmallVector` can impose
a hard bound or inline common-case storage. Comparisons are templated, allowing a stored owning key to be queried with a
non-owning view when the types support comparison.

@snippet Tests/Libraries/Containers/VectorMapTest.cpp VectorMapSnippet

`VectorSet::insert` treats an already present value as success; it only returns `false` if inserting a new value fails.

@snippet Tests/Libraries/Containers/VectorSetTest.cpp VectorSetSnippet

# Stable Addresses With Generation-Checked Handles

`ArenaMap<T>` solves a different problem from `Vector<T>`: inserted objects stay at a stable address and are recovered
in constant time through `ArenaMapKey<T>`. A key carries an index and a generation, so removing an object invalidates
old handles even when that slot is later reused.

The tradeoff is explicit capacity. The arena must be resized while empty, insertion fails when all slots are occupied,
and its sparse storage reserves room for the chosen maximum. This makes it suitable for registries and object graphs
whose references must survive insertion and removal, not as a general-purpose dynamically growing map.

@snippet Tests/Libraries/Containers/ArenaMapTest.cpp ArenaMapSnippet

# Other Building Blocks

`VirtualArray<T>` reserves a maximum virtual address range and commits pages as the logical size grows. This keeps the
base address stable without committing the full capacity up front, but it consumes virtual address space, depends on
platform virtual-memory facilities, and cannot grow beyond the reservation. Use it when pointer stability and a known
maximum dominate the portability and reservation costs. The Async Web Server example uses virtual arrays to reserve
bounded storage for connections, stream queues, headers, and request buffers without letting later growth move them.

`StrongID<Tag, Integer, InvalidValue>` makes identifiers from different domains distinct C++ types while retaining a
compact integer representation. The tag is intentionally empty; it prevents accidentally passing, for example, an
object identifier where a connection identifier is required.

The library also contains small generic algorithms used by these containers. They operate on iterator or span-like
ranges and remain separate from the choice of owning storage.

# Boundaries With Neighboring Libraries

[Memory](@ref library_memory) provides the segment allocation machinery, global and thread-local allocators, and
virtual-memory reservation used here. Containers adds typed element lifetime and collection semantics on top. Choosing
`Vector` therefore opts into allocation through Memory; choosing `Array` does not.

The non-owning `Span<T>` and `StringView` types used across SC APIs are views, not Containers. Prefer those at API
boundaries when the callee does not need ownership; this lets callers keep data in SC containers, STL containers, or
other storage. Owning `String` and byte-oriented `Buffer` follow related storage ideas but belong to Memory; the
Strings library builds text processing and formatting on top of those foundations.

[Containers Reflection](@ref library_containers_reflection) is a separate adapter library. Add it only when these
containers must participate in [Reflection](@ref library_reflection) or SC serialization; Containers itself does not
depend on that machinery.

# Allocation, Lifetime, And Failure

The contiguous containers share the Memory library's segment machinery: size and capacity metadata are stored with a
contiguous element region, and element construction, movement, and destruction follow the selected storage policy.
That common implementation is why `Vector`, `Array`, and `SmallVector` can offer closely matching operations.

The practical rules are:

- check every fallible mutation;
- use `Array` when heap allocation must be impossible;
- remember that `SmallVector` can allocate and move its elements in either direction across its inline boundary;
- do not retain pointers into a vector-backed container across operations that can change capacity;
- choose `ArenaMap` or a suitably reserved `VirtualArray` when stable addresses are the actual requirement.

# Blog

Some relevant blog posts are:

- [February 2025 Update](https://pagghiu.github.io/site/blog/2025-02-28-SaneCppLibrariesUpdate.html)

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
LOC counts exclude comments. Library counts files physically under `Libraries/Containers`.
Single File counts
`SaneCppContainers.h`.
Standalone counts `SaneCppContainersStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 1007		| 3		| 1010	|
| Single File | 1522		| 157		| 1679	|
| Standalone  | 3088		| 1340		| 4428	|
