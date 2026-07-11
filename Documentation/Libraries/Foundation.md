@page library_foundation Foundation

@brief 🟩 Primitive types, asserts, compiler macros, Function, Span, Result

[TOC]

[SaneCppFoundation.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFoundation.h) is the
small, allocation-free vocabulary shared by the other Sane C++ Libraries. It provides primitive and platform types,
non-owning views, explicit error propagation, fixed-storage callbacks, assertions, compiler portability macros, and a
few ownership tools for native resources.

Foundation is useful on its own when those constraints are desirable, but it is primarily the seam that lets the rest
of SC expose public C++ APIs without the STL, exceptions, RTTI, system headers, or hidden heap allocation. Detailed API
reference remains available in the @ref group_foundation topic; this page explains the choices a caller needs to make.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Foundation.svg)


# The mental model

Foundation separates *access* from *ownership*.

- `Span<T>` is a pointer and an element count. `StringSpan` adds an explicit ASCII, UTF-8, UTF-16, or native encoding
  and records whether a terminator follows the viewed bytes. Neither type owns or extends the lifetime of its data.
  `StringPath` is the deliberate exception: it owns one fixed-capacity, native-encoded path buffer.
- `Result` is a `[[nodiscard]]` success/failure value. `SC_TRY` forwards a failure without exceptions. It deliberately
  carries only a stable ASCII error-message pointer, not an arbitrary error payload.
- `Function<Signature>` stores a free function, member binding, functor, or lambda inline. Its default callable storage
  is `2 * sizeof(void*)`; an oversized capture is a compile-time error rather than a heap allocation.
- `UniqueHandle` owns one native-style handle and releases it deterministically. `Deferred` runs local cleanup at scope
  exit. `OpaqueObject` reserves platform-sized inline storage so a public header can hide an implementation defined in
  a `.cpp` file without allocating a PIMPL.

The common thread is fixed, visible cost. A caller supplies the memory behind a view, chooses the lifetime of a callback
capture, and sees failure in the return type. Foundation does not turn borrowed data into owned data behind the API.

# Representative use

`Function` is representative of the library's approach: several callable forms share one value type, but the storage
budget stays part of the type and is checked at compile time.

@snippet Tests/Libraries/Foundation/FunctionTest.cpp FunctionMainSnippet

For buffer-oriented APIs, pass an array or `(pointer, count)` as a `Span<T>`. Slicing reports invalid ranges instead of
creating an unchecked view. A mutable span permits mutation of the referenced objects; a `Span<const T>` does not.
In both cases, the backing array must outlive every use of the span.

`StringSpan` is intentionally narrower than a general string library. It is suitable for passing encoded text and
native paths through low-level APIs, and can compare ASCII, UTF-8, and UTF-16 code points. It does not own, normalize,
format, or tokenize the text. Its conversion support is limited to writing native-encoded, null-terminated text into
caller-provided storage; it is not a general-purpose Unicode conversion API.

# Allocation, errors, and lifetime

Foundation itself performs no dynamic allocation. That promise has practical consequences:

- A `Span` or `StringSpan` can dangle. Returning one is safe only when its backing storage remains alive. `StringPath`
  instead owns its buffer, but rejects content beyond its platform-specific `MaxPath` capacity.
- A `Result` created with `Result::Error("literal")` is self-contained enough for normal propagation because the literal
  has static storage. `Result::FromStableCharPointer` instead makes the caller responsible for keeping the message
  pointer valid until it is inspected.
- A `Function` owns its inline functor or lambda, but references captured by that callable still follow normal C++
  lifetime rules. Increase the second template argument for a deliberately larger inline budget, or capture a pointer
  to longer-lived state; there is no allocating fallback.
- `UniqueHandle` is move-only. `detach()` stops the wrapper from releasing the handle; because it does not return the
  raw value, the caller must already retain that value. Its destructor cannot report a release failure, so code that
  needs that result should call `close()` explicitly.
- `OpaqueObject` trades heap-free encapsulation for a fixed ABI budget. Each platform size and alignment must cover the
  private object; growing that private object beyond the declared budget requires changing the public definition.

These are low-level tools, not runtime safety mechanisms: bounds are carried by `Span`, but `operator[]` is not a checked
accessor, and borrowed lifetime remains a design obligation.

# Where Foundation stops

Foundation has no library dependencies and is intentionally smaller than its neighboring SC libraries:

- Use [Memory](@ref library_memory) when data must grow dynamically or be owned through an explicit allocator (`Buffer`,
  `String`, or `SmallString`). Depending on Foundation alone is a useful signal that a library cannot allocate through
  SC Memory; Foundation's own ownership types use fixed inline storage or native handles instead.
- Use [Containers](@ref library_containers) for owning typed collections such as `Vector`; continue to expose `Span` at
  boundaries when ownership does not need to cross them.
- Use [Strings](@ref library_strings) for `StringView`, iteration, conversion, formatting, parsing, and path manipulation.
  `StringSpan` exists so lower-level libraries can carry encoded text without taking that larger dependency.
- Platform libraries build their public interfaces from `Result`, `Span`, `Function`, `UniqueHandle`, and
  `OpaqueObject`, keeping operating-system types and headers in implementation files.

Foundation is therefore a good fit for small, portable API boundaries with caller-controlled storage. It is not a
replacement for owning containers, rich diagnostics, general Unicode processing, or dynamically sized type erasure.

# Portability layer

Foundation also supplies SC primitive aliases, compiler/feature detection, export and warning macros, assertions,
placement construction, type traits, and small move/forward utilities. These are mostly infrastructure for SC headers,
not an attempt to reproduce the full standard library. See [Compiler Macros](@ref group_foundation_compiler_macros) and
the @ref group_foundation reference when integrating at that level.

# Maturity

🟩 Usable. Foundation is exercised across the supported compilers and platforms because almost every SC library
builds on it. Its surface grows when another library needs a dependency-free primitive; it is not intended to accumulate
convenience APIs speculatively.

# Blog

Some relevant blog posts are:

- [January 2025 Update](https://pagghiu.github.io/site/blog/2025-01-31-SaneCppLibrariesUpdate.html)
- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)
- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)
- [May 2026 Update](https://pagghiu.github.io/site/blog/2026-05-31-SaneCppLibrariesUpdate.html)
- [June 2026 Update](https://pagghiu.github.io/site/blog/2026-06-30-SaneCppLibrariesUpdate.html)

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
