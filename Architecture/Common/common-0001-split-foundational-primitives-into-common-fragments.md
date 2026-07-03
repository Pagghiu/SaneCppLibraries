# COMMON-0001 - Split Foundational Primitives Into Common Fragments

Status: Accepted
Date: 2026-07-03

## Context

Many libraries need foundational primitives such as `Result`, `Span`, compiler macros, platform macros, `Function`, `StringSpan`, or `StringPath`. Historically many of these lived in Foundation, which made Foundation appear as the natural dependency for almost every library even when only one small primitive was needed.

## Decision

Foundational primitives that are useful across multiple libraries may live in `Libraries/Common` as source fragments. Consumers include the specific fragment they need instead of depending on the whole Foundation library. Foundation may still aggregate these fragments for users who want the full foundational set.

## Consequences

Libraries can reduce dependency edges and single-file payloads by including only the fragments they actually need. Common becomes more important than a normal private folder, so its files need stricter include, layout, and single-file rules. Foundation becomes a facade over many primitives rather than the required source of all of them.

## Confirmation

A change preserves this decision when new shared primitives can be consumed without adding a Foundation dependency, Common fragments remain source material rather than a build target, and Foundation can still aggregate the public foundational set.

## Related

- [SC-0010 - Treat Common as source sharing, not a library](../Global/sc-0010-treat-common-as-source-sharing-not-a-library.md)
- [FOUNDATION-0001 - Keep Foundation as the public facade for Common primitives](../Foundation/foundation-0001-keep-foundation-as-the-public-facade-for-common-primitives.md)
- [Libraries/Common agent guidelines](../../Libraries/Common/AGENTS.md)
