# Strings Architecture

## Purpose

`Strings` provides allocation-free string views, iteration, formatting, conversion, command-line parsing, console text helpers, and path algorithms. It should make text encoding explicit and should produce output into caller-owned storage rather than owning storage itself.

## Architectural Shape

The library is built around immutable views and explicit output. `StringView` inspects existing text. `StringIterator` variants walk ASCII, UTF-8, and UTF-16 text. `StringBuilder`, `StringFormat`, `StringConverter`, and `Path` write new text through `IGrowableBuffer`/`GrowableBuffer<T>` adapters. Owned strings live in `Memory`, and fixed native path storage lives in `Common`.

Text APIs should keep encoding visible. Path APIs should parse into views and compose into caller-provided output.

## Boundaries

`Strings` owns string algorithms, formatting, conversion, command-line parsing, console text behavior, and path parsing/composition. It does not own string storage, allocator policy, fixed native path storage, or low-level string/path primitives needed by other libraries.

Low-level libraries should be able to use `StringSpan`, `StringPath`, or `IGrowableBuffer` without depending on `Strings`.

## Similarities With Other Libraries

Like other Sane libraries, `Strings` avoids hidden allocation, STL containers, exceptions, RTTI, and broad dependencies. It uses Common primitives directly and exposes failure through result-like or boolean operations.

## Differences From Other Libraries

Unlike `Memory`, `Strings` must stay allocation-free. Unlike `Common`, it is allowed to provide higher-level parsing, formatting, conversion, and path algorithms. Unlike typical string libraries, it does not center the design on a mutable owning string class.

## Inspirations

The evidenced inspiration is the project's bring-your-own-output pattern through `IGrowableBuffer`. The encoding model is also grounded in platform reality: POSIX commonly uses UTF-8 while Win32 APIs commonly use UTF-16.

## Anti-Inspirations

Inference: `Strings` is deliberately not `std::string` plus utilities. Owned mutable strings, hidden growth, and allocator policy belong outside this library.

Inference: `Strings` is not trying to be a complete Unicode text engine. Grapheme clusters, word breaking, normalization, and case conversion are explicit future work, not accidental guarantees of ordinary iteration.

Inference: `Path` should not become an owned path object model. It should remain a parser/composer over views and caller-owned output.

## Architectural Choices

Keep `Strings` free of a `Memory` dependency.

Keep owned `String` and `SmallString` in `Memory`.

Keep `StringView` immutable and non-owning.

Keep output-producing APIs on `IGrowableBuffer` or `GrowableBuffer<T>`.

Keep ASCII, UTF-8, UTF-16, and native encoding explicit in views and output configuration.

Keep path parsing view-based and path composition caller-owned.

## Explicitly Excluded Targets

Do not add hidden dynamic allocation to string formatting, conversion, console, or path operations.

Do not move `String`, `SmallString`, or allocator-backed string ownership into `Strings`.

Do not require `Strings` when a lower-level library only needs `StringSpan`, `StringPath`, or fixed output growth.

Do not imply grapheme, normalization, or full Unicode case behavior from code-point iteration APIs.

## Sources

- [Strings documentation](../../Documentation/Libraries/Strings.md)
- [SC-0012 - Support bring-your-own containers](../Global/sc-0012-support-bring-your-own-containers.md)
- [COMMON-0007 - Keep IGrowableBuffer as the minimal output-growth adapter](../Common/common-0007-keep-igrowablebuffer-as-the-minimal-output-growth-adapter.md)
- [COMMON-0008 - Keep StringSpan and StringPath in Common](../Common/common-0008-keep-stringspan-and-stringpath-in-common.md)
- [STRINGS-0001 - Keep Strings allocation-free and output through GrowableBuffer](strings-0001-keep-strings-allocation-free-and-output-through-growablebuffer.md)
- [STRINGS-0002 - Treat Strings as immutable views plus builders](strings-0002-treat-strings-as-immutable-views-plus-builders.md)
- [STRINGS-0003 - Make encoding explicit: ASCII, UTF-8, UTF-16, and Native](strings-0003-make-encoding-explicit-ascii-utf8-utf16-and-native.md)
- [STRINGS-0004 - Keep Path as view parsing plus caller-owned composition](strings-0004-keep-path-as-view-parsing-plus-caller-owned-composition.md)
- [StringView](../../Libraries/Strings/StringView.h)
- [StringBuilder](../../Libraries/Strings/StringBuilder.h)
- [StringConverter](../../Libraries/Strings/StringConverter.h)
- [Path](../../Libraries/Strings/Path.h)
- [Strings tests](../../Tests/Libraries/Strings)
- [InteropSTL tests](../../Tests/InteropSTL)

## Decision Log

- [STRINGS-0001 - Keep Strings allocation-free and output through GrowableBuffer](strings-0001-keep-strings-allocation-free-and-output-through-growablebuffer.md)
- [STRINGS-0002 - Treat Strings as immutable views plus builders](strings-0002-treat-strings-as-immutable-views-plus-builders.md)
- [STRINGS-0003 - Make encoding explicit: ASCII, UTF-8, UTF-16, and Native](strings-0003-make-encoding-explicit-ascii-utf8-utf16-and-native.md)
- [STRINGS-0004 - Keep Path as view parsing plus caller-owned composition](strings-0004-keep-path-as-view-parsing-plus-caller-owned-composition.md)
