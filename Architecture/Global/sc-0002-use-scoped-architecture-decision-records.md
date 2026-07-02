# SC-0002 - Use Scoped Architecture Decision Records

Status: Accepted
Date: 2026-07-02

## Context

Sane C++ Libraries has years of architectural decisions spread across code, documentation, blog posts, commit history, local agent guidelines, and maintainer memory. The project is now primarily maintained with AI agents, so implicit rationale is easy to lose and easy for future generated code to contradict.

## Decision

Architecture decisions are recorded as Markdown ADRs under `Architecture/`. Project-wide decisions use the `SC-NNNN` scope in `Architecture/Global/`. Library-specific decisions use their own scope and subfolder, such as `HTTP-NNNN` or `ASYNC-NNNN`. Numbering is local to each scope. When a decision changes, the old ADR remains and is marked as superseded by a newer ADR.

## Consequences

Future agents have a stable place to look for rationale before changing project shape. Library ADRs can refine global decisions without forcing one global numbering queue. The cost is a little more documentation work when decisions are architecture-relevant, but that cost is intentional because it prevents silent drift.

## Confirmation

A change preserves this decision when new architecture-relevant choices are either linked to an existing ADR or captured in a scoped ADR, decision logs stay updated, and old decisions are superseded rather than rewritten to hide history.

## Related

- [Architecture decision format](../README.md)
- [Agent guidelines](../../AGENTS.md)
