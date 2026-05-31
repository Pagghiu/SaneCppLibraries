# Agent Guidelines for Libraries/Common

`Libraries/Common` is not a Sane C++ library. It is a source-sharing folder for small private implementation utilities that individual libraries can inline into their own implementation namespaces.

## Role

- Use this folder only for code that would otherwise need to be duplicated across multiple libraries.
- Files here are source fragments, not public headers and not a dependency target.
- Consumers include these files from private `.cpp` or internal implementation files.
- Each consuming library owns its own private copy after inclusion, especially in generated single-file libraries.
- Prefer keeping code local to one library unless at least two libraries need the same implementation detail.

## File Rules

- Include the normal copyright and SPDX license header in every source fragment.
- Do not use `#pragma once`.
- Do not use include guards.
- Do not include system headers or project headers from Common fragments.
- Do not declare top-level namespaces in Common fragments.
- Do not add public API declarations here.
- Do not put ABI-affecting storage/layout decisions here; those belong in the library that owns the type.

## Consumer Rules

- Include Common fragments only from private implementation files, never from public headers.
- Wrap each include in a library-specific private namespace before including it.
- Do not expose Common type names in public APIs.
- Do not make library dependency metadata point at `Common`; it is intentionally not a library.
- Keep `ToolsBootstrap.c` standalone. It must not depend on `Libraries/Common`.

## Single-File Libraries

- Amalgamators may inline Common fragments into each consuming single-file library.
- Generated single-file libraries must not reference `SaneCppCommon`.
- Generated single-file libraries must not leave raw `#include "Libraries/Common/..."` directives behind.
- It is expected that Common code is duplicated across different single-file outputs.
