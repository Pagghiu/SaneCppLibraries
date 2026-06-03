# Agent Guidelines for Libraries/Common

`Libraries/Common` is not a Sane C++ library. It is a source-sharing folder for code that is shared without creating a library dependency. Some files are guarded header-only definition fragments that public library headers may include; other files are unguarded implementation-only fragments that private `.cpp` or internal files inline into their own implementation.

## Role

- Use this folder only for code that would otherwise need to be duplicated across multiple libraries, or for Foundation pieces being split out so other libraries can use them without depending on Foundation.
- Files here are source-sharing fragments, not a build target and not a dependency target.
- Each consuming library owns its own copy after inclusion, especially in generated single-file libraries.
- Prefer keeping code local to one library unless at least two libraries need the same type, macro, helper, or implementation detail.
- Common code must keep the normal Sane C++ constraints: no STL, no exceptions, no RTTI, no allocation in library code, and no dependency on another Sane C++ library.

## File Kinds

- Include the normal copyright and SPDX license header in every source fragment.
- Guarded `.h` files are header-only definition fragments. They may define reusable `SC::` types, templates, inline functions, and macros that are safe to expose from public library headers.
- Unguarded `.inl` files are implementation-only fragments. They are source material for private implementation namespaces and may define non-inline functions or private helper types for each consuming library's copy.
- Do not use `#pragma once` in Common files.
- Guarded `.h` files must use the existing versioned guard pattern:
  `#ifdef SC_FOUNDATION_NAME_DEFINITION_H`, version check, then `#define ... 1`.
- Increment a guarded header's version macro only when changing it in a way that must reject mixing old and new copies in one translation unit.
- Unguarded `.inl` files must intentionally have no include guard. Add a short comment explaining the required include context when the file is not self-evident.

## Guarded Header Rules

- Guarded headers may include other guarded Common headers.
- Avoid new includes from Common back into `Libraries/Foundation`; migrate existing ones away as Foundation disappears.
- Do not include system headers from guarded Common headers. Public library headers must remain OS-header free.
- Guarded headers may declare `namespace SC` and may expose `SC::` names intended to behave as shared foundational types.
- Keep storage/layout choices stable and intentional. If a Common type appears in a public API, changing its layout changes every consuming library's ABI surface.
- Include guarded headers directly from public or private consumers; do not wrap them in a private namespace.

## Implementation Fragment Rules

- Include unguarded `.inl` fragments only from private `.cpp` or internal implementation files, never from public headers.
- Wrap implementation-only includes in a library-specific private namespace when the fragment defines helper types or functions that should not expose global `SC::` symbols.
- For `Assert.inl`, each consuming library must declare its own provider type with `SC_DECLARE_ASSERT_PROVIDER`, define its own `SC_${LIBRARY}_ASSERT_RELEASE`, `SC_${LIBRARY}_ASSERT_DEBUG`, and `SC_${LIBRARY}_TRUST_RESULT` macros near the library export macro, then include `Assert.inl` once from that library's implementation with `SC_ASSERT_PROVIDER` set to the provider type.
- The consuming implementation file is responsible for including required system headers, platform headers, and project headers before an `.inl` fragment.
- If an `.inl` fragment must use OS APIs or system headers, keep those headers out of the fragment unless the fragment is itself only included from private implementation files and documents that requirement clearly.
- Do not expose implementation-fragment type names in public APIs.

## Consumer Rules

- Do not make library dependency metadata point at `Common`; it is intentionally not a library.
- Do not add generated project entries for `Common`.
- Keep public headers OS-header free even when they include guarded Common headers.
- Keep `ToolsBootstrap.c` standalone. It must not depend on `Libraries/Common`.

## Single-File Libraries

- Amalgamators may inline Common fragments into each consuming single-file library.
- Generated single-file libraries must not reference `SaneCppCommon`.
- Generated single-file libraries must not leave raw `#include "Libraries/Common/..."` directives behind.
- It is expected that Common code is duplicated across different single-file outputs.
- Guarded `.h` fragments must still behave correctly when several included library headers pull in the same Common definition.
- Unguarded `.inl` fragments must be copied into each consuming library implementation only where that library intentionally owns the resulting symbols.
