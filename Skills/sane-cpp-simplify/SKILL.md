---
name: sane-cpp-simplify
description: Propose substantial simplification rounds for Sane C++ Libraries through redundant-code removal, duplicate implementation and test consolidation, and coherent file/example organization, without new inter-library dependencies.
---

# Sane C++ simplification rounds

Read root and scope-local AGENTS.md. Preserve public APIs and the absolute prohibition on new inter-library dependencies. This is a proposal-first workflow: a round authorizes investigation and journal updates, not source edits, deletions, renames, commits, or pushes. Implement only the user's selected proposal IDs. Skill refinement is allowed when requested.

## What qualifies

Look for removal of whole unnecessary mechanisms or test sections, multiple implementations reduced to one, or a coherent organization rule replacing inconsistent names and locations. Tens to hundreds of net lines can be useful evidence; count the abstraction and migration overhead too. A naming/reorganization proposal may save zero lines if it materially improves discoverability or removes special cases.

Do not present isolated repeated assertions, unused variables, comments, or one-to-three-line cleanups as a successful round. Bundle incidental cleanup into an independently worthwhile approved change or leave it out. Conversely, do not manufacture deletion candidates or weaken coverage to meet a line quota.

Start with a repository-wide inventory of file sizes, substantial repetition, registrations, example discovery and naming families. For a broad round, divide independent runtime, tests/protocols, and tools/examples scopes among parallel workers (typically three within available slots), then have the parent verify and consolidate the best findings. Aim for a useful batch spanning libraries; do not fill a quota with trivial or speculative items. Large files and textual similarity are leads, not proof.

Useful SC-specific categories:

- Whole ineffective/subsumed tests: identify the actual oracle, realistic defect it can detect, and surviving coverage. Distinguish assertion-free compile/crash coverage from a test that never exercises its claimed behavior.
- Duplicate test infrastructure: response collectors, chunked streams, platform fixtures, repeated scenario setup. Consolidate mechanics in narrowly scoped test-only helpers; keep operation/phase cases and section selection explicit. Common lifecycle assertions may live inside a narrow scenario runner when failures still identify the case. Do not create a generic test framework for two copies.
- Duplicate production implementation inside one library: keep public wrappers, factor common mechanics privately, and preserve error categories, cleanup, ownership and platform behavior.
- Cross-library similarity: never solve it by making either library depend on the other. Only consider existing `Libraries/Common` source-fragment conventions after reading its AGENTS.md, with each consumer owning its compiled copy. Reject the extraction if adaptation machinery or coupling outweighs the saving. Architecture changes need an ADR.
- Obsolete infrastructure and transitional layers: check registrations, generators, docs, supported platforms and replacement behavior before removing a whole old path.
- File/example organization: define a consistent purpose or API-based scheme and show a before/after map. Separate tutorials, benchmarks, experiments and application hosts where that distinction is useful. Propose names explicitly; keep public API stable and identify command/path compatibility separately.

## Delegation and review

Select models by capability and cost tier from the available provider, without fixed model names or versions. Use a medium-tier model for substantive candidate discovery and coverage analysis, with moderate reasoning effort where supported. Use a light-tier model for well-defined inventories, reference searches and duplicate verification, or a later bounded audit if it demonstrates useful results; higher reasoning effort can be appropriate for a cheap bounded task. Reserve the strong tier for consequential unresolved questions. Calibrate worker quality using comparable scopes and instructions; an unsuccessful narrow audit does not establish a general model-quality comparison. Record model, effort, scope and outcome; prefer useful findings per cost over token price alone. Avoid unnecessary parallel agents or repeated audits of unchanged code.

Give auditors explicit scope, relevant instructions, a small number of substantive findings, and read-only/no-build constraints. Each finding must state the resulting simpler structure, what disappears, retained behavior, estimated net savings, dependency impact, counterarguments and validation. The parent independently verifies source and callers; findings are not approval.

For rare consequential uncertainty that source review and workers cannot resolve, use a persistent top-level same-project strong-tier advisor, with low or moderate reasoning effort where supported. Explain the unresolved question first, inspect available chats/projects, and reuse a suitable advisor when authorized. Keep the advisory prompt narrow, forbid edits, and record its chat ID. Do not use a strong-tier advisor for routine cleanup or rubber-stamping.

## SC verification traps

For scenario consolidation, HTTP wire helpers, stored callbacks or runtime stalls, read [SC simplification review patterns](references/test-simplification.md). Compare observable behavior per scenario; assertion totals and textual similarity alone do not establish equivalent coverage.

- Read `Libraries/Testing/Testing.h`: `SC_TEST_EXPECT` evaluates expressions; Result checks can execute setup and cleanup. Tests with similar shape can cover different ownership, capacity, error, cancellation, encoding or platform contracts. Prefer consolidating fixture mechanics over deleting distinct scenarios.
- An uncalled helper may be a Doxygen `\snippet` source or template/compile coverage. Search helper names and snippet markers. Repository-local absence of callers never proves a public API is dead.
- Check `Tests/SCTest/SCTest.cpp`, `Tools/SC-build.cpp` (exact tracked casing), platform guards, examples, documentation and generators. Never introduce a production dependency to save test code. Shared HTTP fixtures stay under Tests, consistent with `Tests/Libraries/Http/AGENTS.md`.
- Example directory names currently determine build targets in `configureExamplesConsole`; `Await` prefixes select C++20, and benchmark names trigger special configuration. A directory move requires discovery changes, includes, documentation/CI/script references, and preservation or explicit approval of CLI name changes. Do not simply nest directories and assume builds still work.
- `SCTest --all-tests` includes BuildTest, but not the separate `SCBuildTest` fixture suite. For build-fixture changes compile/run SCBuildTest in both configurations. If a full fixture run stops before the edited sections, preserve that failure and run the affected sections explicitly; a focused pass does not close the broader gate.
- AsyncFibers runtime-owned stack coverage requires Fil-C with glibc; ordinary native compilers report it unsupported. Record this limit rather than claiming both stack modes passed.
- Generated single-file copies are intentionally duplicated. Reduce maintained source rather than deleting generated output. Check Python/JavaScript amalgamator parity when relevant; generated consumers must stay independently usable.

## Journal and proposal packet

The primary review surface is the persistent `_Plans/Simplification/index.html`, rendered from `opportunities.json`; `Log.md` retains detailed evidence/history. Read [the dashboard contract](references/dashboard.md) when updating or using it. Update the dashboard after every round, grouped by library/function, with completed work archived below active opportunities. All operational files stay ignored; verify with `git check-ignore`, never force-add. Preserve IDs and prior decisions. Agents return findings; the parent owns the board and journal.

Record date, revision/dirty state, scope, models, baseline, stable proposal IDs, source anchors, evidence, proposed structure, coverage preserved/lost, dependency impact, risks, gross deletion/addition and estimated net saving, approval status and validation. Statuses: proposed, approved, rejected, deferred, implemented, verified. Put uncertain discoveries in a backlog with the next concrete question; do not silently expand a round. A round finding no defensible substantial changes is valid.

Rank the batch by maintenance benefit, confidence and risk. Each dashboard row explains the code purpose, touched files, change/reason, net LOC or clarity benefit, evidence, risks and next action. Mark unresolved exploration purple and concrete user questions amber. For consequential technical uncertainty source/workers cannot settle, consult the authorized persistent strong-tier advisor or ask the user through an explicit opinion item; record the answer before implementation. Distinguish ready implementation proposals from architectural investigations. Do not count a code move as deletion, promise speculative savings, or aggregate overlapping proposals. Present a coherent batch for selection instead of asking approval for trivial edits individually.

## Metrics and approved implementation

Run `python3 Skills/sane-cpp-simplify/scripts/measure.py` before and after approved work; save outputs under the ignored journal folder. Keep source roots/extensions fixed, include new untracked maintained source in the net count, and report source savings separately from skill/docs overhead and repository text totals. Record intentional category moves so moving code outside a metric root cannot masquerade as simplification. Baselines measure working-tree contents at the recorded HEAD.

Keep approved rounds reviewable in Git: commit each independently meaningful area separately, including its formatting and related test changes. Commit reusable skill/workflow changes separately from library and test refactors. Record proposal IDs and resulting commit hashes in the ignored journal; do not put that journal in commits.

After approved implementation, follow AGENTS.md: format and inspect alignment, compile before execution, run full Debug and Release suites, regenerate/compile single-file libraries, and use reachable platform VMs described under `/Users/stefano/Developer/Agents/VM`. If a documented VM is stopped, start it through Parallels Desktop using computer use; the user has authorized this for validation. Recheck reachability before requesting help. Isolate network ports. Report BuildTest exclusions and unavailable platforms honestly. Source-only audits need no C++ suites. An authorized runtime-failure investigation may compile and run bounded reproductions or diagnostic copies; preserve failures and distinguish this evidence from validation of an approved implementation.

End every turn with a dashboard link and a concise recommended next batch or investigation; avoid repeating the full table in chat. Use stable IDs; distinguish investigation from already-developed implementation proposals. Completing a batch is not a reason to omit the next choices. These suggestions do not authorize implementation or starting another round.

For audit results, put the ranked proposals and estimated versus achieved savings in the dashboard, then link it in chat. Record implementation and validation before advancing to another round.

## Feedback after every round

End each investigation or implementation round with a brief skill-feedback entry in the journal. Check what the audit missed, which candidates survived parent review, where estimates or validation assumptions were wrong, and whether a newly observed codebase pattern suggests another useful issue/simplification category. Propose a concrete skill tweak with its supporting evidence and when it should apply; mention the useful proposal in the closing response alongside the next batch. If nothing warrants a change, explicitly record that outcome instead of inventing a rule.

Treat skill tweaks as proposals until authorized. When refining the skill, prefer correcting or merging existing guidance over adding another checklist; keep conditional detail in a linked reference. Teach future agents what patterns to look for, how to recognize them, and what contracts or verification traps to check. Keep round IDs, results and incident history in the ignored journal; include a specific example in the skill only when it clarifies a reusable pattern. Distinguish reusable patterns from one-off platform incidents and unresolved hypotheses. As the workflow settles, fewer changes are expected: each round still considers feedback, but need not modify the skill. Codebase categories can grow as evidence accumulates, without silently broadening an approved implementation round.
