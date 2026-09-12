# Methodology and initial result status

## Question

Does the revised API skill improve correct composition of Sane APIs versus no skill and its original Git revision, and does the separate Style skill transfer explicit-resource design beyond Sane APIs?

## Design

API comparisons use `no-skill`, Git-recovered `original`, and current `optimized` under equal task/model/reasoning/source/OS/time conditions. Style comparisons use a separate non-Sane library task with `no-style` and `style`. Development tasks support revision; held-out tasks freeze before comparison. Acceptance tests and source-review obligations are written before inspecting a submission. See [the protocol](../protocol.md).

No-skill has no skills directory in the staged checkout, while repository docs and root `AGENTS.md` remain available to all conditions. This controls the explicit skill route, but unrestricted-host execution cannot fully prevent an agent from navigating to another checkout; each run records this limitation.

## Measures

For every trial, save the first submission and acceptance JSON before any external feedback. Record full-task success, individual checks, phase (`first_submission`, `autonomous_repair`, or `evaluator_assisted_repair`), wall time, model/reasoning, source/skill hashes, available tool/build/test counts, and actual runtime telemetry. Runtime tokens/cost remain unavailable unless emitted by the runtime. Source review is independent evidence, not a test result.

## Initial result

There are zero newly executed comparison trials (`n=0`). The statement “the optimized skill improves outcomes” is therefore unsupported. The initial iteration completed the skills, Git-addressed API baseline, staged-condition runner, task contracts, acceptance dispatch, review rubrics, and validation. The independent Style acceptance harness was smoke-tested against its private conforming fixture; that validates the harness only, not either skill. It stops here until fresh subject capacity is available.

## Planned bounded continuation

Smoke the development traversal task once per API condition with Luna High. If the harness is sound, run three fresh held-out attempts per API condition and three Style/no-style attempts on the standalone event log. Treat these as descriptive samples with uncertainty, investigate test defects separately, and do not expand further without a material finding.
