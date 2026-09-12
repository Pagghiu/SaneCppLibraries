# Historical evidence kept outside the comparison

## Evaluation 1: original async case study

The read-only presentation experiment in the external `AgentBenchmark` workspace used source revision `e2594fc43fb9b4cbee5d308b41a270e84833e0e6`, Luna High, a fresh task context, and the original API skill. It is a one-run async job-runner case study, not a skill-condition comparison. The source report remains in that workspace; this repository does not copy it.

Its corrected evaluator reports 5/6 functional suites passing for both ordinary and ASan/UBSan builds. The failing large-output case reproduced child stdout/stderr inheriting nonblocking pipe behavior, leading to short output. Source review separately found that the implementation used 64 logical job/request sets rather than reusing `N` slots, had incomplete cleanup on partial setup/event-loop failure, and treated read errors as EOF; the latter paths were not fault-injected. Actual concurrency and ordinary timeout/reaping were observed. Earlier evaluator concurrency errors were corrected and preserved.

This evidence informs task coverage—pipe behavior, observable slot reuse, partial cleanup, and error classification—but is never shown to a subject and is not counted as `original`, `optimized`, or `no-skill` performance.

## Evaluation 2: old and updated skills

The same read-only presentation workspace contains an independent two-submission report for this evaluation. On the same six-suite evaluator, the frozen old API-skill submission passed 3/6 suites and the submission given updated API plus Style skills passed 4/6. Neither completed the task. Ordinary and ASan/UBSan builds reproduced those patterns without sanitizer diagnostics. These are one attempt per condition, and the combined updated condition cannot isolate the API skill's effect from the Style skill's.

The old submission used four physical slots but retained stale `Process` argument state across reuse. Its subject accidentally saw two prohibited lines from SC-build source during a framework-link search; the exposure is a protocol deviation, with no evidence of copying the motivating implementation. The updated submission reset `Process` state and passed large-output, reuse, and input checks, but both quiet deadline cases hung because its clock check could not wake `runOnce`. The evaluator terminated the runner and cleaned orphaned fixture children. Source review also found blocking pipe reads on a worker pool, and normal macOS child reaping was not established. Those source findings are distinct from observed suite failures.

This rerun motivates general guidance about timed waits, blocking I/O strategy, state reset, and reaping. It remains external case-study evidence and is excluded from the in-repository pilot ledger and any before/after rate.
