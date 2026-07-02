# Architecture Decisions

This directory records architectural decisions for Sane C++ Libraries.

ADRs use scoped identifiers so decisions can be referenced across libraries without one global numbering queue:

- `SC-NNNN`: project-wide decisions shared by all libraries.
- `JSON-NNNN`: Json library decisions.
- Future library scopes should use short uppercase slugs, for example `MEM-NNNN`, `CONT-NNNN`, or `ASYNC-NNNN`.

## ADR Format

Use one decision per file:

```md
# SCOPE-NNNN - Short Decision Title

Status: Accepted
Date: YYYY-MM-DD

## Context

What forces made this decision necessary.

## Decision

What we decided, in active voice.

## Consequences

What becomes easier, harder, or deliberately out of scope.

## Confirmation

How agents and reviewers can tell whether code still follows this decision.

## Related

Optional links to plans, docs, or related ADRs.
```

Rules:

- Keep IDs scoped by directory. Do not maintain one project-wide numeric sequence.
- Use lowercase filenames with the scope prefix: `json-0001-short-slug.md`.
- Keep ADRs short. Prefer one page or less.
- Record decisions that are hard to reverse, surprising without context, or likely to be debated again.
- Do not edit history when a decision changes. Add a new ADR and mark the old one as superseded.
- Use `Confirmation` for checks, tests, scripts, code-review questions, or documentation cues that help agents preserve the decision.
- Put implementation task lists in `Plans/`, not here.

Reference formats:

- MADR: https://adr.github.io/madr/
- Nygard ADR: https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions

## Scopes

- [Global](Global/README.md): project-wide decisions with `SC-NNNN` identifiers.
