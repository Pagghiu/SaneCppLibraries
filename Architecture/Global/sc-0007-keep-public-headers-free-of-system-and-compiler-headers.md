# SC-0007 - Keep Public Headers Free of System and Compiler Headers

Status: Accepted
Date: 2026-07-02

## Context

Public headers are the first integration surface for repository users, generated single-file users, and agents inspecting APIs. Including operating-system, compiler, or standard-library headers from public Sane headers increases compile time, leaks platform details, and can make no-stdlib or cross-platform builds fail for reasons unrelated to the public API.

## Decision

Public library headers must not include operating-system headers or compiler-provided system headers. Public types should use Sane primitives, forward declarations, opaque storage, or small common fragments. Platform headers belong in `.cpp` files or private internal implementation files.

## Consequences

Public APIs need stable primitive types and explicit opaque storage for handles or platform state. Some implementation details require extra private translation-unit work instead of convenient inline code. In return, public headers stay cheap to include, portable, and friendlier to single-file generation.

## Confirmation

A change preserves this decision when public headers compile in isolation without OS or system headers, generated single-file public sections stay clean, and platform-specific includes appear only in implementation or internal files intended for that purpose.

## Related

- [Project principles](../../Documentation/Pages/Principles.md)
- [Coding style: Public Headers](../../Documentation/Pages/CodingStyle.md#public-headers)
- [Foundation documentation](../../Documentation/Libraries/Foundation.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
