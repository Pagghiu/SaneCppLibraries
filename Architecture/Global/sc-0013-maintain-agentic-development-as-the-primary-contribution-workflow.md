# SC-0013 - Maintain Agentic Development as the Primary Contribution Workflow

Status: Accepted
Date: 2026-07-02

## Context

Sane C++ Libraries is now maintained primarily through AI agents. Code can be generated quickly, but generated changes need durable intent, reproducible prompts, local constraints, and human sanity checks so the architecture does not drift.

## Decision

The project treats agentic development as the primary contribution workflow. Issues, Prompt Requests, reusable prompts, local `AGENTS.md` files, skills, ADRs, tests, and validation commands are part of the architecture control surface. Code-only human pull requests are not the preferred path, and agent-produced code must include enough context for a maintainer or future agent to reproduce, review, or adapt the change.

## Consequences

Good issues and prompts become first-class contributions. Documentation and local guidelines need to be precise enough for agents, not just humans. The maintainer may replay or adapt contributed prompts rather than merging code as-is. This creates process overhead, but it keeps generated code accountable to project intent.

## Confirmation

A change preserves this decision when contribution guidance, local agent instructions, ADRs, and tests stay aligned; substantial generated code includes validation context; and new workflows improve agent reproducibility instead of bypassing it.

## Related

- [Contributing: Contribution model](../../CONTRIBUTING.md#contribution-model)
- [Agent guidelines](../../AGENTS.md)
- [Sane C++ skill](../../Skills/sane-cpp-libraries/SKILL.md)
- [SC-0002 - Use scoped architecture decision records](sc-0002-use-scoped-architecture-decision-records.md)
