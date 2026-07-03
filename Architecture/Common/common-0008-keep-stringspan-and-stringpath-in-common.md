# COMMON-0008 - Keep StringSpan and StringPath in Common

Status: Accepted
Date: 2026-07-03

## Context

Many low-level libraries need string views, native path views, fixed path buffers, encoding metadata, or path-shaped output, but they should not depend on Strings or Memory just to accept or produce names. Moving every path helper to Strings would reintroduce dependencies into File, Process, Plugin, Testing, and other platform libraries.

## Decision

`StringSpan` and `StringPath` live in Common as foundational non-owning or fixed-storage string/path primitives. Higher-level string formatting, conversion, normalization, and allocation-capable string ownership stay in the appropriate libraries.

## Consequences

Path-heavy libraries can remain allocation-free and independent while still sharing one small path representation. Common must keep these primitives conservative and avoid turning them into the full Strings library. Encoding and native path behavior become part of the shared low-level API surface.

## Confirmation

A change preserves this decision when low-level libraries can use string/path primitives without depending on Strings or Memory, and additions to `StringSpan` or `StringPath` do not pull in allocation, formatting, or broad string-library policy.

## Related

- [StringSpan](../../Libraries/Common/StringSpan.h)
- [StringPath](../../Libraries/Common/StringPath.h)
- [COMMON-0006 - Treat Common public layouts as cross-library API surface](common-0006-treat-common-public-layouts-as-cross-library-api-surface.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
