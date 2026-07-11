@page library_memory Memory

@brief 🟩 Explicit allocation policy, owning byte and string storage, and virtual memory

[TOC]

[SaneCppMemory.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppMemory.h) is the
allocation layer used by the SC libraries that need owning, dynamically sized storage. It provides fallible byte
buffers, owning encoded strings, replaceable allocators, and a cross-platform virtual-memory reservation.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Memory.svg)


# When Memory Is The Right Layer

Most SC APIs prefer borrowed `Span` and `StringView` values and do not allocate behind their public interface. Use the
Memory library when ownership or growth is genuinely required and the caller must be able to see, bound, redirect, or
measure that allocation.

The library is a good fit when you need:

- an owning byte buffer without adopting the STL;
- inline storage for the common case, with an explicit heap fallback;
- an owning, null-terminated string that retains its encoding;
- a temporary allocation domain backed by fixed caller-owned memory;
- a large, stable virtual address range whose physical pages are committed incrementally.

It is not a general object-ownership framework. There are no shared or unique smart pointers, and the design
deliberately discourages graphs of separately allocated objects with unclear lifetimes. Applications can still use
their own containers and allocators at their boundaries; a library that does not depend directly or indirectly on
Memory should not perform runtime allocation.

# The Mental Model: Storage Policy Is Visible

There are three layers to keep distinct:

1. `Memory` is the low-level allocate, reallocate, release, copy, move, and set interface routed through the current
   global allocator.
2. `MemoryAllocator` is a replaceable allocation policy. `FixedAllocator` serves allocations from a finite contiguous
   region, while `VirtualAllocator` grows through committed pages of a `VirtualMemory` reservation.
3. Owning values such as `Buffer` and `String` use the selected global or thread-local allocator when their current
   storage is insufficient.

The default global allocator ultimately uses the regular heap. Replacing it is scoped with `Globals::push` and
`Globals::pop`; it is not a parameter passed through every container operation. Regular `Buffer` and `String` use the
global domain. Their `BufferTL`, `SmallBufferTL`, and related thread-local aliases select the thread-local domain.

Allocator replacement is ownership-aware. An allocator may refuse to grow storage created by a different allocator,
even if it has enough spare capacity. This prevents silently moving an existing allocation into an unrelated domain,
but it means pushing a new allocator does not retroactively migrate live buffers. Establish the intended domain before
constructing the values that belong to it.

# Byte Storage: `Buffer` And `SmallBuffer`

`Buffer` is an owning contiguous sequence of bytes. `SmallBuffer<N>` presents the same `Buffer` interface but starts
with `N` inline bytes. Crossing `N` can allocate; shrinking the logical size does not immediately return to inline
storage, while `shrink_to_fit()` can move the contents back when they fit.

@snippet Tests/Libraries/Memory/BufferTest.cpp BufferBasicSnippet

This distinction matters when evaluating allocation guarantees: `SmallBuffer<N>` is a common-case optimization, not
a hard bound. A move can steal a heap block, and growth can invalidate pointers and spans into either buffer. If heap
allocation must be impossible, supply a fixed allocator with enough storage and handle failure rather than relying on
the word “Small”.

Operations that may grow—such as `resize`, `reserve`, `append`, and `assign`—return `[[nodiscard]] bool`. Copy
constructors and assignment operators cannot report allocation failure and therefore assert if the copy cannot be
completed. Prefer the fallible operations on paths where exhaustion is expected or recoverable. `clear()` only resets
the logical size; use `shrink_to_fit()` when releasing excess storage is important.

# Owning Strings Are Here For A Reason

`String` belongs to Memory rather than [Strings](@ref library_strings) because ownership requires allocation. It owns a
null-terminated byte sequence and records its `StringEncoding`; `SmallString<N>` adds inline byte capacity with the
same heap-fallback tradeoff as `SmallBuffer<N>`. The non-owning string algorithms and views remain in Strings.

Call `view()` to borrow a `StringSpan`. That view is valid only while the source string remains alive and unmodified;
`assign` and other storage-moving operations invalidate it. Size is measured in bytes, and
`sizeInBytesIncludingTerminator()` deliberately includes the encoding's terminator bytes. This is not a Unicode text
editing abstraction: use the Strings library for viewing, conversion, construction, and text-oriented operations.

As with buffers, fallible `assign` is preferable when allocation failure must be handled. Convenience constructors and
assignment operators assert if the required allocation fails.

# Put A Bound Around A Subsystem

`FixedAllocator` is a monotonic-style allocator over a caller-provided memory region. It can release or reallocate the
most recent allocation efficiently, but it is not a general-purpose heap with arbitrary reclamation. Exhaustion is a
normal failure result. This makes it useful for a request, parsing pass, test, or other scope where the total budget is
known and all values die together.

The current allocator is installed through a `Globals` object:

@snippet Tests/Libraries/Memory/GlobalsTest.cpp GlobalsSnippetFixed

The allocator, its backing bytes, and every object allocated from them must outlive all users. Always balance
`Globals::push` with `Globals::pop`; a small deferred cleanup is appropriate on paths with multiple early returns.
Global replacement has no locking and is unsafe to mutate concurrently. Use the thread-local domain when each thread
needs an independent policy, and call `Globals::init(Globals::ThreadLocal, settings)` in each such thread when custom
ownership tracking is required.

Every `MemoryAllocator` exposes counts for allocate, reallocate, and release calls. Those counters are useful for
tests and coarse instrumentation, but they report calls rather than current or peak bytes.

# Reserve Address Space, Commit Physical Memory Later

`VirtualMemory` reserves a contiguous address range and commits page-rounded portions on demand. The base address stays
stable while the committed size changes, which is valuable for large arrays, arenas, or registries that know their
maximum size but cannot tolerate relocation. Reservation consumes virtual address space; committed pages consume
physical memory and OS bookkeeping. This is primarily a 64-bit technique, not a universally cheaper heap.

@snippet Tests/Libraries/Memory/VirtualMemoryTest.cpp VirtualMemorySnippet

`reserve`, `commit`, and `decommit` are fallible, sizes are rounded to the platform page size, and memory must not be
read or written beyond the committed prefix. `VirtualMemory` is non-copyable and non-movable. Its destructor releases
the reservation; an explicit `release()` remains useful when the address space should be returned before scope exit.

`VirtualAllocator` adapts that reservation to the `MemoryAllocator` interface and commits pages as its contiguous
allocation frontier grows:

@snippet Tests/Libraries/Memory/GlobalsTest.cpp GlobalsSnippetVirtual

Like `FixedAllocator`, it favors a group of allocations with a shared lifetime over arbitrary per-object reclamation.
The maximum reservation is a hard capacity, and live values must be destroyed before the `VirtualAllocator` and its
`VirtualMemory` backing object.

# Relationship To Neighboring Libraries

- The header-only Common primitives supply non-owning spans, string spans, placement construction, and related
  building blocks; Memory adds ownership and allocation policy.
- [Strings](@ref library_strings) supplies non-owning text views, conversion, and builders. `String` and `SmallString`
  live here because they own storage.
- [Containers](@ref library_containers) builds typed contiguous collections such as `Vector<T>` and `SmallVector<T>`
  on the same segment and allocator machinery. Prefer `Buffer` for bytes to avoid template and executable-size cost;
  use Containers when element construction, destruction, and typed collection operations matter.
- `GrowableBuffer<T>` adapters let libraries such as File write directly into `Buffer` or `String` storage without
  depending on their concrete implementation. The adapter finalizes the logical size when the write completes.

# Status And Limits

🟩 Usable

The core buffer and allocation paths are established and used by downstream libraries. The candid limitations are the
global-domain coordination required by custom allocators, the assertion-based behavior of infallible-looking copy
operations, and the intentionally narrow allocator implementations. This library is designed for explicit budgets and
coherent lifetimes, not as a drop-in replacement for the full C++ allocator and smart-pointer ecosystem.

# Further Reading

These development notes cover the evolution and extraction of the Memory library:

- [February 2025 Update](https://pagghiu.github.io/site/blog/2025-02-28-SaneCppLibrariesUpdate.html)
- [March 2025 Update](https://pagghiu.github.io/site/blog/2025-03-31-SaneCppLibrariesUpdate.html)
- [April 2025 Update](https://pagghiu.github.io/site/blog/2025-04-30-SaneCppLibrariesUpdate.html)
- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)

# Roadmap

The library grows when downstream code exposes a concrete need. Smart pointers are intentionally unplanned: the
[project principles](@ref page_principles) prefer grouped allocations and explicit ownership over large numbers of
independently allocated objects or shared lifetime graphs.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Memory`.
Single File counts
`SaneCppMemory.h`.
Standalone counts `SaneCppMemoryStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 554		| 1053		| 1607	|
| Single File | 1566		| 1183		| 2749	|
| Standalone  | 1566		| 1183		| 2749	|
