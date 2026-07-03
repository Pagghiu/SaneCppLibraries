# COMMON-0006 - Treat Common Public Layouts As Cross-Library API Surface

Status: Accepted
Date: 2026-07-03

## Context

Common public fragments define types that appear in many public APIs, such as `Result`, `Span`, `StringSpan`, `StringPath`, `Function`, `UniqueHandle`, `OpaqueObject`, and `IGrowableBuffer`. Changing their storage, alignment, size, or semantics can affect every consuming library and every generated single-file output.

## Decision

Public Common definitions are treated as cross-library API surface. Layout, alignment, ownership, and lifetime semantics must be stable and intentional. Changes that affect public layout or behavior require broad validation and, when architecture-relevant, a superseding ADR or linked library-specific ADR.

## Consequences

Common public types should stay small, boring, and explicit. Optimizations or convenience additions need to be weighed against ABI/API churn across all consumers. This makes Common slower to change than private implementation code, but it protects independent library adoption.

## Confirmation

A change preserves this decision when public Common type layout changes are deliberate, tests cover representative consumers, single-file libraries still compile, and documentation or ADR links explain any cross-library impact.

## Related

- [COMMON-0002 - Use guarded headers for shared public definitions](common-0002-use-guarded-headers-for-shared-public-definitions.md)
- [SC-0014 - Use automated checks to protect architecture](../Global/sc-0014-use-automated-checks-to-protect-architecture.md)
