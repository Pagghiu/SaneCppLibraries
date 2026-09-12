# Skill evaluation: bounded initial iteration

This directory evaluates two different claims without conflating them:

- `sane-cpp-libraries` helps compose real Sane APIs.
- `sane-cpp-style` transfers explicit-resource design practices to code that does not use Sane APIs.

The original API skill is recovered on demand from the Git revision recorded in `skill-versions.json`; the candidate skills come from `Skills/`. Every staged trial records its exact Git revision, dirty state, and skill-tree hash. This keeps Git as the version store instead of committing duplicate skill trees. The supplied historical `AgentBenchmark` is read-only external evidence, summarized in [historical evidence](reports/historical-evidence.md). It is not included in any before/after denominator.

## Initial stopping point

The repository contains a ready-to-run pilot: three API-skill conditions, a separate Style transfer task, predeclared acceptance checks, source-review criteria, staging, and result validation. No subject has run this in-repository pilot (`n=0`). A later external rerun of the original process task is summarized in [historical evidence](reports/historical-evidence.md); neither submission completed the task.

Run `python3 scripts/validate_benchmark.py` to validate metadata, the recoverable Git baseline, task manifests, condition isolation rules, local skill links, and absence of committed binaries. The Style reference, timer-slot, and file-copy protocol fixtures under `fixtures/` smoke-test the independent harness; the protocol fixtures only exercise evaluator input/output contracts. All staged trials, submissions, test output, and executables live under `_Build/SkillEvaluation`. Use the command template in [protocol](protocol.md) to launch a bounded pilot once fresh subject capacity is available.

## Layout

- `tasks/development`: revise skills against these tasks; do not use for headline results.
- `tasks/held-out`: freeze before the comparison; subjects must not see solution material or review conclusions.
- `skill-versions.json`: Git baseline and version-recording policy; no copied skills.
- `scripts`: staging, metadata validation, and acceptance-test entry points.
- `_Build/SkillEvaluation`: generated trial records, submissions, acceptance output, and binaries; not committed.
- `reports`: method, historical evidence, and presentation-ready factual summary.

The proposed pilot is one fresh Luna High attempt per API condition on one development task after the harness is smoke-validated. A useful next bounded batch is three fresh attempts per condition on the held-out file-copy task; Style is evaluated separately with three fresh attempts. The timer-slot task was revised after the external rerun and therefore moved to development; record the task hash in future trials. Expand only after reviewing variance, harness defects, and failures.
