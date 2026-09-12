# Historical evidence kept outside the comparison

## Evaluation 1: original async case study

The read-only presentation experiment in the external `AgentBenchmark` workspace used source revision `e2594fc43fb9b4cbee5d308b41a270e84833e0e6`, Luna High, a fresh task context, and the original API skill. It is a one-run async job-runner case study, not a skill-condition comparison. The source report remains in that workspace; this repository does not copy it.

Its corrected evaluator reports 5/6 functional suites passing for both ordinary and ASan/UBSan builds. The failing large-output case reproduced child stdout/stderr inheriting nonblocking pipe behavior, leading to short output. Source review separately found that the implementation used 64 logical job/request sets rather than reusing `N` slots, had incomplete cleanup on partial setup/event-loop failure, and treated read errors as EOF; the latter paths were not fault-injected. Actual concurrency and ordinary timeout/reaping were observed. Earlier evaluator concurrency errors were corrected and preserved.

This evidence informs task coverage—pipe behavior, observable slot reuse, partial cleanup, and error classification—but is never shown to a subject and is not counted as `original`, `optimized`, or `no-skill` performance.
