# SC-0015 - Prefer Maturing Existing Libraries Over Expanding Scope

Status: Accepted
Date: 2026-07-02

## Context

The repository already contains many platform abstraction libraries at different maturity levels. AI agents are good at generating plausible new modules, but adding libraries increases documentation, testing, dependency, single-file, CI, and maintenance load.

## Decision

The project prefers maturing the existing library set over adding new libraries. New library proposals are out of scope unless the maintainer explicitly decides otherwise through discussion and, for architecture-relevant additions, an ADR.

## Consequences

Agents should focus on hardening current APIs, filling platform gaps, improving tests, clarifying docs, reducing dependencies, and moving libraries from Draft or MVP toward Usable. Useful ideas that do not fit the current scope should become issues, notes, or future proposals rather than immediate implementation.

## Confirmation

A change preserves this decision when it improves an existing library, tool, example, documentation path, or validation surface; and when new top-level libraries are not added without explicit maintainer approval and architecture documentation.

## Related

- [Contributing: Do not expand project scope](../../CONTRIBUTING.md#do-not-expand-project-scope)
- [Project status](../../Documentation/Pages/Index.md#status)
- [README Libraries](../../README.md#libraries)
