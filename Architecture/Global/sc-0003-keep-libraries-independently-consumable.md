# SC-0003 - Keep Libraries Independently Consumable

Status: Accepted
Date: 2026-07-02

## Context

Sane C++ Libraries is meant to be a collection of libraries that work well together, not a framework that users must adopt as a whole. Internal dependencies make single-library adoption harder, enlarge single-file outputs, hide allocation behavior, and give agents more opportunities to introduce accidental coupling.

## Decision

Each library should remain independently consumable where practical. New dependencies between Sane C++ libraries require explicit design discussion and should be avoided unless the dependency provides more value than the independence it removes. Shared implementation should not automatically become a library dependency.

## Consequences

Library APIs often use small common primitives, caller-provided storage, callbacks, or lightweight adapters instead of reaching for a convenient higher-level Sane library. Some code may be duplicated or shaped awkwardly to preserve independence. In exchange, users can adopt individual libraries without pulling in the whole ecosystem, and the dependency graph remains small enough for humans and agents to reason about.

## Confirmation

A change preserves this decision when dependency reports do not show new accidental library edges, single-file outputs remain reasonable, and new cross-library dependencies are either absent or justified by a linked ADR.

## Related

- [README Libraries](../../README.md#libraries)
- [Contributing: No accidental internal dependencies](../../CONTRIBUTING.md#no-accidental-internal-dependencies)
- [SC-0004 - Single-file libraries are first-class distribution artifacts](sc-0004-single-file-libraries-are-first-class-distribution-artifacts.md)
- [SC-0010 - Treat Common as source sharing, not a library](sc-0010-treat-common-as-source-sharing-not-a-library.md)
