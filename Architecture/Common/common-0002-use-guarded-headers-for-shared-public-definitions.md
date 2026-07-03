# COMMON-0002 - Use Guarded Headers for Shared Public Definitions

Status: Accepted
Date: 2026-07-03

## Context

Some Common fragments define public `SC::` types, templates, macros, and inline helpers that may be included by several public library headers in the same translation unit or generated single-file output. Ordinary `#pragma once` behavior is not enough to detect incompatible duplicated copies after amalgamation or mixed-version inclusion.

## Decision

Common `.h` files that define shared public declarations use explicit versioned include guards such as `SC_FOUNDATION_RESULT_DEFINITION_H`. They do not use `#pragma once`. The version macro is incremented only when a change must reject mixing old and new copies in one translation unit.

## Consequences

Common guarded headers can safely appear in public library headers and generated single-file libraries. The guard names still use the historical `SC_FOUNDATION_*` form because these definitions originated as Foundation primitives and remain Foundation-exported public concepts. The cost is a little more guard boilerplate and care when changing layout or semantics.

## Confirmation

A change preserves this decision when new public Common `.h` files use the versioned guard pattern, include only other safe Common guarded headers when needed, avoid system headers, and still behave correctly when several library headers include the same definition.

## Related

- [Libraries/Common agent guidelines](../../Libraries/Common/AGENTS.md)
- [COMMON-0006 - Treat Common public layouts as cross-library API surface](common-0006-treat-common-public-layouts-as-cross-library-api-surface.md)
- [SC-0007 - Keep public headers free of system and compiler headers](../Global/sc-0007-keep-public-headers-free-of-system-and-compiler-headers.md)
