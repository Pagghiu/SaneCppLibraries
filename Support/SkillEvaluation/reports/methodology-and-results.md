# Methodology and initial result status

## Question

Does the revised API skill improve correct composition of Sane APIs versus no skill and its original Git revision, and does the separate Style skill transfer explicit-resource design beyond Sane APIs?

## Design

API comparisons use `no-skill`, Git-recovered `original`, and current `optimized` under equal task/model/reasoning/source/OS/time conditions. Style comparisons use a separate non-Sane library task with `no-style` and `style`. Development tasks support revision; held-out tasks freeze before comparison. Acceptance tests and source-review obligations are written before inspecting a submission. See [the protocol](../protocol.md).

No-skill has no skills directory in the staged checkout, while repository docs and root `AGENTS.md` remain available to all conditions. This controls the explicit skill route, but unrestricted-host execution cannot fully prevent an agent from navigating to another checkout; each run records this limitation.

## Measures

For every trial, save the first submission and acceptance JSON before any external feedback. Record full-task success, individual checks, phase (`first_submission`, `autonomous_repair`, or `evaluator_assisted_repair`), wall time, model/reasoning, source/skill hashes, available tool/build/test counts, and actual runtime telemetry. Runtime tokens/cost remain unavailable unless emitted by the runtime. Source review is independent evidence, not a test result.

## Initial result

The in-repository controlled pilot still has zero executed trials (`n=0`). External evaluation 2 of the original runner yielded 3/6 suites for the old API skill and 4/6 for updated API plus Style skills, with full-task failure in both. The outcome is summarized in [historical evidence](historical-evidence.md); detailed results remain in the external `AgentBenchmark` workspace. One attempt per condition, combined skills in the updated condition, and a disclosed old-condition source-exposure deviation prevent a measured skill-effect claim. Runtime tokens and cost are unavailable.

The skill revision after that rerun adds source-backed guidance for a deadline that wakes `runOnce`, distinguishing worker-thread blocking reads from native nonblocking reads, resetting stateful process objects on slot reuse, and verifying OS reaping. The timer-slot task now has an independently defined quiet-deadline case and a failure-continuation case, so it is a development regression rather than a held-out task. A separate bounded file-copy task is held out for generalization, which remains unmeasured.

## Planned bounded continuation

Smoke the development traversal task once per API condition with Luna High. If the harness is sound, run three fresh held-out attempts per API condition and three Style/no-style attempts on the standalone event log. Treat these as descriptive samples with uncertainty, investigate test defects separately, and do not expand further without a material finding.
