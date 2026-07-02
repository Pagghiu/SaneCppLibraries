# SC-0017 - Publish Draft and MVP Libraries Deliberately

Status: Accepted
Date: 2026-07-02

## Context

The project publishes libraries before every API is stable or complete. This can look surprising for a reusable library project, especially to agents that may assume public code should either be stable or hidden. The staged maturity model lets related libraries evolve together and gives users visibility into useful but incomplete work.

## Decision

Libraries may be public in Draft, MVP, Usable, or Complete states. Draft and MVP status are deliberate signals, not documentation debt. Public documentation should identify library maturity and relevant roadmap or follow-up work, and architecture-sensitive instability should be recorded where it affects future decisions.

## Consequences

Users and agents can distinguish incomplete design from accidental neglect. APIs in Draft or MVP libraries may change more freely than Usable libraries, but changes still need tests, documentation updates, and respect for global architecture constraints. Maturity promotion should reflect behavior, tests, platform support, and documentation, not just implementation volume.

## Confirmation

A change preserves this decision when library status remains visible in documentation, unstable areas are named honestly, and agents do not treat Draft or MVP status as permission to bypass project-wide rules.

## Related

- [Project status](../../Documentation/Pages/Index.md#status)
- [README Libraries](../../README.md#libraries)
- [SC-0015 - Prefer maturing existing libraries over expanding scope](sc-0015-prefer-maturing-existing-libraries-over-expanding-scope.md)
