@page library_strings Strings

@brief 🟩 Non-owning encoded text, formatting, conversion, parsing, and path manipulation

[TOC]

[SaneCppStrings.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppStrings.h) is the
text-processing layer used across Sane C++ Libraries. It keeps encoding and null-termination information alongside a
byte range, then provides comparison, slicing, tokenization, formatting, conversion, command-line parsing, console
output, and platform-aware path operations without imposing an owning string type.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Strings.svg)


# Status
🟩 Usable  
The core ASCII, UTF-8, and UTF-16 operations are in regular use across the project. Unicode support is intentionally
limited to code-point operations and encoding conversion; it is not a full Unicode text-processing stack.

# Start With The Text Model

The central type is `StringView`: a read-only range of bytes plus three pieces of information needed to interpret it:
its byte length, its `StringEncoding`, and whether a terminator immediately follows the range. A view does not own or
copy the text. Slices and tokens are more views into the same storage, so they remain valid only while that storage is
alive and unmoved.

`StringEncoding` distinguishes ASCII, UTF-8, and little-endian UTF-16. `StringEncoding::Native` is UTF-16 on Windows and
UTF-8 elsewhere, matching the encodings used at the operating-system boundaries in the rest of SC. The size reported
by a view is in bytes, not characters. Use `StringIteratorASCII`, `StringIteratorUTF8`, or `StringIteratorUTF16` when an
operation must advance by Unicode code point.

This model is a good fit for systems code that already knows where text lives and needs explicit platform conversion.
It is a poor fit for code that expects string values to own themselves, silently grow, or provide locale-aware natural
language operations.

# Views Are Cheap, But Their Contracts Matter

`StringView` compares different supported encodings by decoded code point and provides searches, prefix and suffix
checks, trimming, splitting, numeric parsing, and tokenization. `StringViewTokenizer` advances through the input and
exposes the current component as another view; it does not create a collection of copied tokens.

There are two practical cautions:

- null termination is tracked separately from the viewed byte range; a slice may not be suitable for a C API even when
  the original string was terminated;
- code-point equality is not Unicode normalization. Canonically equivalent spellings can compare unequal, and
  case-insensitive helpers are explicitly ASCII-only.

`StringSpan`, defined in the common foundation used by SC libraries, carries the same basic range, encoding, and
termination metadata with a deliberately smaller operation set. APIs use it when they only need to transport text.
`StringView` derives from it and adds the richer Strings algorithms. This keeps lower-level libraries from depending on
Strings merely to pass a path or message.

# Build Into Storage Chosen By The Caller

Text is never mutated through a view. `StringBuilder` instead writes a new sequence into an object supported by
`GrowableBuffer`: an owning `String`, an inline `SmallString<N>`, a byte `Buffer`, or compatible caller-provided storage.
The destination determines whether growth allocates, uses inline capacity, or fails at a hard limit.

For a one-shot result, format directly into the destination:

@snippet Tests/Libraries/Strings/StringBuilderTest.cpp stringBuilderFormatSnippet

For incremental output, create a builder, check every `append`, and call `finalize` before using its resulting view.
The typed builder also finalizes in its destructor, but explicit finalization makes the failure boundary and resulting
lifetime easier to see.

@snippet Tests/Libraries/Strings/StringBuilderTest.cpp stringBuilderTestAppendSnippet

Formatting uses indexed `{}` placeholders and supports the built-in scalar, pointer, and string types declared by
`StringFormat`. It is intentionally not `std::format`: unsupported types need an SC formatter specialization, and a
format or write error is reported as `false`. Ignoring that result can turn insufficient destination capacity into
truncated or missing output.

# Convert Only At Encoding Boundaries

`StringConverter::appendEncodingTo` decodes the source and appends ASCII, UTF-8, or UTF-16 output to caller-selected
storage. It can also append the destination encoding's null terminator for an operating-system or C API. If no
conversion or terminator is needed, higher-level platform code can often pass the original `StringSpan` through
instead of allocating a temporary.

@snippet Tests/Libraries/Strings/StringConverterTest.cpp stringConverterTestSnippet

The destination's declared encoding is not changed by conversion. In particular, constructing an owning `String` with
the intended encoding is part of the call-site contract. Plain byte buffers carry no encoding metadata at all, so the
caller must use the same encoding when constructing a view over their contents and must exclude any terminator from the
viewed byte count.

# Paths Are Lexical Strings, Not Filesystem Operations

`Path` parses and composes both POSIX and Windows syntax regardless of the host platform. It can split a path into
views, select `dirname` or `basename`, test whether a path is absolute, join components, normalize `.` and `..`, and
compute a relative path. Operations taking `Path::AsNative` select Windows syntax on Windows and POSIX syntax elsewhere.

Parsing methods return views into the original path. Joining and normalization write to caller-provided storage and
can fail. Normalization is lexical: it does not access the filesystem, resolve symbolic links, verify case, or prove
that the resulting path exists. Its template workspace has a default limit of 64 components; choose a larger explicit
bound when valid inputs can exceed that.

Use [FileSystem](@ref library_file_system) when the job is to query or change actual files. FileSystem APIs exchange
native-encoded spans with `Path`, but Strings itself performs no I/O.

# Command Lines And Console Output

`CommandLineSpec` describes options and positional values using caller-owned spans. Parsing writes directly into the
provided variables, reports structured help/error/success status, and never builds a hidden argument collection.
Repeated positional values therefore require the caller to provide `Span<StringSpan>` storage. `CommandLineArguments`
adapts `main`/`wmain` arguments without changing the production API's view-based model.

`Console` is a thin formatted stdout/stderr writer. Its format string must be ASCII or UTF-8. On Windows, an optional
caller-provided conversion buffer is used at the native console boundary; callers that need predictable behavior for
long converted messages should size that workspace deliberately.

# Allocation, Lifetime, And Failure

Strings has no direct dependency and does not choose an allocator. That does not mean every use is allocation-free:
passing `String` or a heap-backed `Buffer` to a builder, converter, or path operation lets that destination grow through
the [Memory](@ref library_memory) library. Passing fixed-capacity storage keeps the operation bounded but makes capacity
failure part of normal control flow.

The rules that matter at call sites are:

- keep the bytes behind every `StringSpan`, `StringView`, token, and parsed path component alive and stationary;
- do not retain views into a growable destination across an operation that may reallocate it;
- keep source and destination separate for path normalization and other rebuilding operations unless the API explicitly
  documents overlap;
- treat `false` from builders, converters, formatters, and path composition as a real failure;
- add a terminator only when the receiving API requires one, and do not include it in the logical view length.

# Boundaries With Neighboring Libraries

[Memory](@ref library_memory) owns `String`, `SmallString<N>`, and `Buffer`, because ownership and allocation are memory
policies rather than text algorithms. Strings can write into those types through the growable-buffer interface without
depending on Memory. This separation is also why including Strings alone does not provide a classic owning string.

[Containers](@ref library_containers) is appropriate when parsed tokens or views must be collected, but remember that a
container of views still does not own the referenced characters. Copy the text into owning storage when it must outlive
the input.

[FileSystem](@ref library_file_system) and [FileSystem Iterator](@ref library_file_system_iterator) perform filesystem
I/O and return native-encoded text. `Path` provides their lexical path manipulation. `Console` provides basic process
output; asynchronous byte transport and buffering belong to [Async Streams](@ref library_async_streams).

# What Is Deliberately Missing

Strings currently works at bytes, code units, and Unicode code points. It does not implement grapheme-cluster
iteration, locale-aware collation, word breaking, Unicode normalization, or general Unicode case conversion. A visible
"character" can contain multiple code points, so code-point iteration is not sufficient for cursor movement or other
human-text UI behavior. Applications needing those semantics should use a dedicated Unicode library at that boundary.

# Blog

Relevant design and implementation updates:

- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)
- [August 2025 Update](https://pagghiu.github.io/site/blog/2025-08-31-SaneCppLibrariesUpdate.html)
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)

# Roadmap

Potential additions remain deliberately uncommitted because correct Unicode behavior is substantially broader than
encoding conversion:

🟦 Complete Features:
- UTF normalization
- UTF case conversion

💡 Unplanned Features:
- UTF word breaking
- Grapheme-cluster iteration

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Strings`.
Single File counts
`SaneCppStrings.h`.
Standalone counts `SaneCppStringsStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 1056		| 3143		| 4199	|
| Single File | 2076		| 3399		| 5475	|
| Standalone  | 2076		| 3399		| 5475	|
