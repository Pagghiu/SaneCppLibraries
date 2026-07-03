# FOUNDATION-0001 - Keep Foundation As the Public Facade for Common Primitives

Status: Accepted
Date: 2026-07-03

## Context

Foundation historically owned the small primitives used throughout the project. Many of those primitives moved to Common so libraries could include only what they need without depending on Foundation. Users still need a coherent Foundation library that exposes the foundational set as one discoverable public surface.

## Decision

Foundation remains the public facade for the foundational primitive set. Its headers may aggregate Common fragments and present the documented Foundation library, but other libraries should include Common fragments directly when that avoids an unnecessary Foundation dependency.

## Consequences

Foundation is useful for users and documentation, while Common is the lower-level source-sharing mechanism for dependency hygiene. Foundation should stay thin and should not accumulate implementation policy merely because a primitive is foundational. New cross-library primitives should be evaluated for Common first, then surfaced through Foundation when they belong in the public foundational set.

## Confirmation

A change preserves this decision when Foundation continues to build and document the foundational set, direct library consumers can still include Common fragments without depending on Foundation, and Foundation does not become a hidden dependency aggregator for unrelated libraries.

## Related

- [Foundation documentation](../../Documentation/Libraries/Foundation.md)
- [COMMON-0001 - Split foundational primitives into Common fragments](../Common/common-0001-split-foundational-primitives-into-common-fragments.md)
- [SC-0010 - Treat Common as source sharing, not a library](../Global/sc-0010-treat-common-as-source-sharing-not-a-library.md)
