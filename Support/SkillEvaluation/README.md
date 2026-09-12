# Skill evaluation: bounded initial iteration

This directory evaluates two different claims without conflating them:

- `sane-cpp-libraries` helps compose real Sane APIs.
- `sane-cpp-style` transfers explicit-resource design practices to code that does not use Sane APIs.

The original API skill is recovered on demand from the Git revision recorded in `skill-versions.json`; the candidate skills come from `Skills/`. Every staged trial records its exact Git revision, dirty state, and skill-tree hash. This keeps Git as the version store instead of committing duplicate skill trees. The supplied historical `AgentBenchmark` is read-only external evidence, summarized in [historical evidence](reports/historical-evidence.md). It is not included in any before/after denominator.

## Initial stopping point

The repository contains a ready-to-run pilot: three API-skill conditions, a separate Style transfer task, predeclared acceptance checks, source-review criteria, staging, and result validation. No new subject trial has run (`n=0`); the available subject-agent quota ended before launch. This is an honest stopping point, not a zero-pass measurement.

Run `python3 scripts/validate_benchmark.py` to validate metadata, the recoverable Git baseline, task manifests, condition isolation rules, local skill links, and absence of committed binaries. The Style acceptance fixture has a conforming reference implementation under `fixtures/style-reference`, used only to smoke-test the independent harness. All staged trials, submissions, test output, and executables live under `_Build/SkillEvaluation`. Use the command template in [protocol](protocol.md) to launch a bounded pilot once fresh subject capacity is available.

## Layout

- `tasks/development`: revise skills against these tasks; do not use for headline results.
- `tasks/held-out`: freeze before the comparison; subjects must not see solution material or review conclusions.
- `skill-versions.json`: Git baseline and version-recording policy; no copied skills.
- `scripts`: staging, metadata validation, and acceptance-test entry points.
- `_Build/SkillEvaluation`: generated trial records, submissions, acceptance output, and binaries; not committed.
- `reports`: method, historical evidence, and presentation-ready factual summary.

The initial proposed pilot is one fresh Luna High attempt per API condition on one development task after the harness is smoke-validated. A useful next bounded batch is three fresh attempts per condition on the frozen held-out API task; Style is evaluated separately with three fresh attempts. Expand only after reviewing variance, harness defects, and failures.
