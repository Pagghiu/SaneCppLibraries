# Controlled evaluation protocol

## Conditions

API tasks use exactly these conditions:

| ID | Skill material | Subject instruction |
| --- | --- | --- |
| `no-skill` | Staged source excludes `Skills/` and `Support/SkillEvaluation`; no skill path is supplied. | Read ordinary repository documentation and the root `AGENTS.md`; do not use a skill. |
| `original` | Staging recovers only the original API skill from the Git revision in `skill-versions.json`. | Read `Skills/sane-cpp-libraries/SKILL.md` before work. |
| `optimized` | Staged source contains the tested current API skill revision. | Read `Skills/sane-cpp-libraries/SKILL.md` before work. |

All conditions get the same task contract, source commit, model/reasoning setting, OS, time limit, ordinary docs, build access, and root `AGENTS.md`. The staged checkout keeps docs and source but excludes historical benchmark evidence and evaluator artifacts. The no-skill workspace physically lacks skill directories; the executor must also launch the subject with that staged directory as its only routed workspace. This runner cannot cryptographically prevent an agent with unrestricted host shell access from finding another checkout, so trial reports must mark filesystem isolation as `best_effort` unless the execution environment enforces it.

For Style tasks, compare `no-style` (no style skill staged) with `style` (only `sane-cpp-style` staged). Do not let API skill access enter this comparison.

## Before launch

1. Freeze task contract, acceptance test, source-review rubric, source revision, skill revision hashes, model/reasoning, time limit, and expected output name.
2. Run the acceptance test against its supplied fixture or a known conforming reference without reading subject output.
3. Create a fresh staged checkout under `_Build/SkillEvaluation` using `prepare_condition.py`; run `validate_benchmark.py`.
4. Give the subject only the task contract and staged path. Do not mention historical runs, expected fixes, reviewer findings, or acceptance implementation details beyond the contract.

## Subject phases and repairs

Record `first_submission` before external evaluation. A subject may autonomously compile and repair during its time limit; record build/test commands if runtime telemetry exposes them. Do not provide evaluator output during that phase.

If a separate evaluator-assisted repair phase is authorized, copy the submission to a new revision, provide only the failing acceptance output, and record it as `evaluator_assisted_repair`; never overwrite the first submission. A run that fails to start, is interrupted, or cannot be evaluated remains in the ledger with its exclusion reason.

## Grading

Run executable acceptance tests first. Then do a blinded source review against the task's `review.md`, without condition identity where practical. Classify each finding as one of `test_defect`, `library_defect`, `skill_omission`, or `implementation_failure`; do not change library behavior to rescue a condition.

Full-task success means every required executable check passes and no blocking source-review obligation remains. Report individual-check rates separately from full-task success. Report denominators, `n`, and uncertainty; for small samples use “descriptive only”, not a reliability claim.

For a task that launches children and requires reaping, predeclare a direct-child reaping acceptance check before subjects run. Keep the runner alive with one slow child while another direct child exits; after a bounded grace period, inspect the exited child's PID, parent PID, and process state during that interval. On POSIX, a persistent `Z`/defunct child whose parent is the live runner fails the reaping requirement. Check ordinary completion as well as timeout/cancellation when required by the task. Absence of the PID after the runner exits is not proof that the runner reaped it. If the platform cannot observe process state, mark reaping unverified rather than inferring success, and clean up probe children on every evaluator exit path.

## Launch template

```sh
python3 Support/SkillEvaluation/scripts/prepare_condition.py \
  --condition optimized --task api-bounded-traversal --run-id api-bounded-traversal-optimized-001 \
  --output _Build/SkillEvaluation/runs

# Run the fresh subject in the emitted stage directory, then save its telemetry and artifacts there.
python3 Support/SkillEvaluation/scripts/run_acceptance.py \
  --task api-bounded-traversal \
  --submission _Build/SkillEvaluation/runs/api-bounded-traversal-optimized-001/submission \
  --build-dir _Build/SkillEvaluation/runs/api-bounded-traversal-optimized-001/build \
  --out _Build/SkillEvaluation/runs/api-bounded-traversal-optimized-001/acceptance.json
```

The runner records unavailable telemetry as `null`; never estimate agent tokens or cost from output size, elapsed time, or account-wide usage.
