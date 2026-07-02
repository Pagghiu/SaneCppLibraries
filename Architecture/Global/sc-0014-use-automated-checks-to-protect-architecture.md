# SC-0014 - Use Automated Checks to Protect Architecture

Status: Accepted
Date: 2026-07-02

## Context

Architecture rules are easy for humans and agents to violate accidentally. Dependency edges, single-file output shape, formatting, warnings, no-stdlib assumptions, and platform behavior can regress even when a local change looks reasonable.

## Decision

Architectural rules should be protected by automated checks where practical. Build commands, tests, dependency reports, single-file compilation, formatting, warnings-as-errors, sanitizers, coverage, and platform CI are part of how the project preserves design intent.

## Consequences

New architecture-sensitive decisions should include a confirmation path that can eventually become a check. Not every rule can be fully mechanized, but agents should prefer checks over prose-only enforcement when the rule is code-inferable. Some validation commands are slower, so local work may use focused checks before broader CI-style validation.

## Confirmation

A change preserves this decision when new architectural constraints have an explicit validation route, existing checks continue to run, and agents report what was and was not validated before handing work back.

## Related

- [AGENTS commands](../../AGENTS.md#commands)
- [Contributing: Testing](../../CONTRIBUTING.md#testing)
- [SC-0002 - Use scoped architecture decision records](sc-0002-use-scoped-architecture-decision-records.md)
- [SC-0004 - Single-file libraries are first-class distribution artifacts](sc-0004-single-file-libraries-are-first-class-distribution-artifacts.md)
