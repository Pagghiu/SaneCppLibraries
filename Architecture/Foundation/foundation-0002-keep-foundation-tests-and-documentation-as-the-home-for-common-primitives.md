# FOUNDATION-0002 - Keep Foundation Tests and Documentation As the Home for Common Primitives

Status: Accepted
Date: 2026-07-03

## Context

Common is intentionally not a library, so it should not gain its own build target or public documentation identity. However, Common fragments still need tests, examples, snippets, and user-facing documentation because they define important public primitives.

## Decision

Foundation remains the main documentation and test home for public Common primitives unless a primitive is better tested through a concrete consuming library. Foundation docs describe the shared foundational types, and Foundation tests may include Common headers directly.

## Consequences

Users can learn `Result`, `Span`, `StringSpan`, `Function`, `UniqueHandle`, `OpaqueObject`, and similar primitives from Foundation documentation even though the files physically live in Common. Tests stay attached to a real library/test grouping instead of creating a fake Common library. Documentation must be clear when a primitive is available as a Common fragment and also surfaced by Foundation.

## Confirmation

A change preserves this decision when public Common primitive behavior is covered by Foundation or representative consuming-library tests, Foundation documentation remains the discoverable user-facing entry point, and no Common build target is introduced just to host tests or docs.

## Related

- [Foundation documentation](../../Documentation/Libraries/Foundation.md)
- [Foundation tests](../../Tests/Libraries/Foundation)
- [COMMON-0001 - Split foundational primitives into Common fragments](../Common/common-0001-split-foundational-primitives-into-common-fragments.md)
