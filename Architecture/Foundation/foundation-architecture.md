# Foundation Architecture

## Purpose

`Libraries/Foundation` is the public facade for foundational Sane C++ primitives. It gives users a single entry point for primitive types, compiler/platform macros, fixed-storage utilities, result-based APIs, assertions, and optional C++ runtime shims while the shareable implementation pieces live in `Common`.

Foundation is intentionally small because almost every other library may include or document concepts that originated here.

## Architectural Shape

Foundation is shaped as a thin facade over Common fragments. `Compiler.h`, `Platform.h`, and `PrimitiveTypes.h` include the relevant guarded Common headers. `Foundation.cpp` wires the Foundation assert provider and conditionally includes the C++ runtime shim implementation when `SC_PROVIDE_CPP_RUNTIME_SHIMS` is enabled.

Tests and documentation remain in Foundation for many primitives that physically live in Common. This preserves a coherent user-facing home for `Span`, `StringSpan`, `StringPath`, `Result`, `Function`, `Deferred`, `OpaqueObject`, `UniqueHandle`, compiler macros, platform macros, and small type traits.

## Boundaries

Foundation owns the public facade, documentation grouping, and Foundation-specific runtime/assert integration. Common owns the reusable fragments that other libraries may include without depending on Foundation.

Foundation must not become a catch-all utility library. New code belongs here only when it is foundational, broadly reusable, dependency-free, and consistent with public-header constraints.

## Similarities With Other Libraries

Foundation follows the global Sane C++ constraints: no hidden allocation, no STL/exceptions/RTTI in library code, explicit `Result` use, caller-owned storage, and OS-header-free public headers.

Like other leaf libraries, Foundation is independently consumable and must keep single-file generation viable. Like Common, it defines vocabulary that other libraries rely on.

## Differences From Other Libraries

Foundation is more facade-like than most libraries. Many implementation and definition fragments now live under `Libraries/Common`, while Foundation remains the documented entry point for users.

Unlike Common, Foundation is a real library scope with its own documentation page, generated artifact, tests, and optional implementation code. Unlike allocation-capable libraries, it should keep foundational behavior static, bounded, and dependency-free.

## Inspirations

The documented inspiration is the project principle that public headers should avoid system and compiler headers while still providing portable primitive types and platform/compiler vocabulary.

Foundation is also shaped by the project's single-file adoption mode: it should remain usable as a standalone downloaded header and as the baseline included by other Sane C++ libraries.

## Anti-Inspirations

Foundation is not intended to become an STL substitute, a runtime framework, or a dependency hub.

Inferred anti-inspirations: it avoids centralizing every convenient helper in one base library, avoids requiring consumers to link a common runtime, and avoids hiding platform-specific implementation details in public headers.

## Architectural Choices

- Keep Foundation as the public facade for Common primitives.
- Keep tests and documentation for foundational primitives under Foundation even after source fragments move to Common.
- Keep C++ runtime shims optional and Foundation-owned.
- Use Common guarded headers for primitive definitions, compiler macros, and platform macros.
- Keep Foundation dependency-free and small enough for broad inclusion.
- Point agents to `Common` when a primitive needs to be shared without a Foundation dependency.

## Explicitly Excluded Targets

- No new inter-library dependencies.
- No system or compiler headers in public Foundation headers.
- No broad utility dumping ground.
- No hidden dynamic allocation.
- No mandatory C++ runtime shim behavior.
- No moving Common-owned source fragments back into Foundation merely for convenience.

## Sources

- [Documentation/Libraries/Foundation.md](../../Documentation/Libraries/Foundation.md)
- [Libraries/Foundation](../../Libraries/Foundation)
- [Libraries/Common](../../Libraries/Common)
- [Tests/Libraries/Foundation](../../Tests/Libraries/Foundation)
- [COMMON-0001 - Split foundational primitives into Common fragments](../Common/common-0001-split-foundational-primitives-into-common-fragments.md)
- [FOUNDATION-0001 - Keep Foundation as the public facade for Common primitives](foundation-0001-keep-foundation-as-the-public-facade-for-common-primitives.md)
- [FOUNDATION-0002 - Keep Foundation tests and documentation as the home for Common primitives](foundation-0002-keep-foundation-tests-and-documentation-as-the-home-for-common-primitives.md)
- [FOUNDATION-0003 - Keep C++ runtime shims optional and Foundation-owned](foundation-0003-keep-cpp-runtime-shims-optional-and-foundation-owned.md)
- Git history around the `Foundation: Move ... to Common` commits.

## Decision Log

- [FOUNDATION-0001 - Keep Foundation as the public facade for Common primitives](foundation-0001-keep-foundation-as-the-public-facade-for-common-primitives.md)
- [FOUNDATION-0002 - Keep Foundation tests and documentation as the home for Common primitives](foundation-0002-keep-foundation-tests-and-documentation-as-the-home-for-common-primitives.md)
- [FOUNDATION-0003 - Keep C++ runtime shims optional and Foundation-owned](foundation-0003-keep-cpp-runtime-shims-optional-and-foundation-owned.md)
