# Common Architecture

## Purpose

`Libraries/Common` is the source-fragment area for foundational code that must be shared without creating a Sane C++ library dependency. It exists so small primitives can be reused by multiple libraries, embedded into single-file outputs, and kept independent from `Foundation`.

Common is not a build target, not a package, and not a dependency. Future agents should treat it as shared source material that each consuming library owns after inclusion.

## Architectural Shape

Common is split into guarded `.h` definition fragments and unguarded `.inl` implementation fragments. Guarded headers expose stable `SC::` definitions such as `Result`, `Span`, `StringSpan`, `StringPath`, `Function`, `UniqueHandle`, compiler macros, and platform macros. Unguarded `.inl` files are included by private implementation files when a consuming library must own the resulting symbols.

The structure mirrors the project's single-file distribution model: Common code may be duplicated across generated libraries, while guarded definitions prevent incompatible repeated definitions inside one translation unit.

## Boundaries

Common may contain primitives, macros, small templates, fixed-layout public types, and implementation fragments that are needed by more than one library. It must not contain product-specific behavior, library-specific policy, generated project metadata, or code that makes another Sane C++ library a dependency.

Public guarded fragments must stay free of system headers. Implementation fragments may rely on include context supplied by the consuming `.cpp`, but their requirements must remain local and explicit.

## Similarities With Other Libraries

Common follows the same project-wide rules as Sane C++ libraries: no STL, no exceptions, no RTTI, no hidden allocation, explicit result propagation, and OS-header-free public surfaces.

Like the leaf libraries, Common uses caller-owned storage and small concrete types. Like `Foundation`, it defines vocabulary used almost everywhere else.

## Differences From Other Libraries

Common is deliberately not a library. It has no independent binary or package identity, no public dependency edge, and no standalone generated target.

Unlike normal libraries, an unguarded Common `.inl` file may be included more than once by different consumers so each consumer gets its own private implementation. Unlike `Foundation`, Common does not provide the documentation facade or assert provider policy for users.

## Inspirations

The strongest evidenced inspiration is the project's own single-file library model: shared definitions must be inlineable into independently consumable artifacts without leaving cross-library includes behind.

The guarded header pattern is also inspired by the need to include the same foundational definition through several public library headers while detecting incompatible version mixes.

## Anti-Inspirations

Common is explicitly not a conventional shared utility library, not a static archive, and not a dependency target.

Inferred anti-inspirations: it avoids the STL-style model where reusable primitives live behind one central runtime library, and it avoids header-only convenience code that quietly pulls system headers or allocation behavior into public APIs.

## Architectural Choices

- Keep Common dependency-free, including from `Foundation`.
- Use guarded `.h` fragments for shared public definitions.
- Use unguarded `.inl` fragments only for private per-consumer implementation.
- Let each consuming library own assert provider configuration.
- Treat public Common layouts as cross-library API surface.
- Keep `IGrowableBuffer`, `StringSpan`, and `StringPath` in Common so allocation-free producers and consumers do not need `Foundation`.

## Explicitly Excluded Targets

- No build target named Common.
- No dependency metadata pointing at Common.
- No generated single-file reference to `SaneCppCommon`.
- No raw `#include "Libraries/Common/..."` left inside generated single-file outputs.
- No system headers in guarded public fragments.
- No implementation-fragment type names in public APIs.
- No new Common fragment unless at least two libraries need the shared type, macro, helper, or implementation pattern.

## Sources

- [Libraries/Common/AGENTS.md](../../Libraries/Common/AGENTS.md)
- [Libraries/Common](../../Libraries/Common)
- [SC-0010 - Treat Common as source sharing, not a library](../Global/sc-0010-treat-common-as-source-sharing-not-a-library.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0004 - Single-file libraries are first-class distribution artifacts](../Global/sc-0004-single-file-libraries-are-first-class-distribution-artifacts.md)
- Git history around the `Foundation: Move ... to Common` commits.

## Decision Log

- [COMMON-0001 - Split foundational primitives into Common fragments](common-0001-split-foundational-primitives-into-common-fragments.md)
- [COMMON-0002 - Use guarded headers for shared public definitions](common-0002-use-guarded-headers-for-shared-public-definitions.md)
- [COMMON-0003 - Use unguarded inl files as per-consumer implementation source](common-0003-use-unguarded-inl-files-as-per-consumer-implementation-source.md)
- [COMMON-0004 - Keep Common free from Foundation and library dependencies](common-0004-keep-common-free-from-foundation-and-library-dependencies.md)
- [COMMON-0005 - Let each consuming library own its assert provider](common-0005-let-each-consuming-library-own-its-assert-provider.md)
- [COMMON-0006 - Treat Common public layouts as cross-library API surface](common-0006-treat-common-public-layouts-as-cross-library-api-surface.md)
- [COMMON-0007 - Keep IGrowableBuffer as the minimal output-growth adapter](common-0007-keep-igrowablebuffer-as-the-minimal-output-growth-adapter.md)
- [COMMON-0008 - Keep StringSpan and StringPath in Common](common-0008-keep-stringspan-and-stringpath-in-common.md)
