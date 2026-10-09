# SC simplification review patterns

Use these criteria when consolidating tests, retiring helpers, extracting fixtures, or investigating runtime stalls. They guide candidate review; they do not expand an approved implementation scope.

## Preserve observable behavior while sharing mechanics

For each test scenario, identify the defect it can detect, the operation that reaches it, and the observable assertion. Look for repeated setup, execution and cleanup around distinct scenarios. Share those mechanics while retaining meaningful case names, section selectors and diagnostic context. Compare operations and observable outcomes before and after; assertion counts alone do not establish equivalent coverage.

Check whether a test proves the contract it names or only a weaker side effect. For example, an HTTP response body does not prove connection reuse or peer closure. Observe the relevant connection lifecycle separately, and distinguish server-side cleanup from peer EOF. Fixture capacity, implicit defaults and cleanup can accidentally determine the behavior under test; ensure the scenario actually reaches the intended path.

## Evaluate whole-helper replacement costs

Before retiring a test utility, inventory its consumers and less obvious capabilities: raw protocol observations, malformed inputs, staged delivery, timing controls and cleanup. Include replacement code in net savings. Prefer a narrow private consolidation when removing the utility would scatter its responsibilities across callers.

Before sharing similar paths, compare initialization, reset fields, error order, ownership and cleanup. In SC, a Result check may execute work, and caller-owned storage makes lifetime and capacity differences significant. Similar-looking branches can encode different contracts. Record suspected bugs separately; behavior normalization requires its own approval.

Separate correctness repairs from simplification benefits. Charge added assertions and helper declarations to net savings. Stronger coverage may justify more code, but that is a correctness benefit, not a deletion claim.

## Check stored callback lifetime when extracting fixtures

Look for callbacks that detach subscriptions, replace their own stored SC::Function, or destroy the object holding them. Reassignment can destroy the executing lambda; accessing captures afterward can use ended lifetimes. Check cleanup order, reentrancy and the lifetime of referenced fixture objects through the final callback. Defer destructive cleanup until callback execution has finished where required. Existing usage is evidence to inspect, not proof that a pattern is safe to copy.

## Diagnose progress before simplifying synchronization

Distinguish a deadlock from healthy work exceeding a cumulative timeout. Repeated sleeps and scheduler handoffs can accumulate platform-dependent delays. Measure elapsed time and meaningful work checkpoints on the affected platform; do not assume a requested sleep duration is its actual duration. A phase label or polling heartbeat may not prove work is advancing.

Use bounded reproductions and isolated temporary diagnostics when useful. Record the revision, instrumentation changes, controlled parameters and remaining environmental uncertainty. Compile before running, separate runner/bootstrap time from test time, and keep compact patches and logs in the ignored journal.

A passing run with a longer deadline is diagnostic evidence, not a synchronization fix or proof of race freedom. If proposing a no-progress watchdog, advance it only at meaningful checkpoints and verify with bounded fault injection that stalled work still times out. Preserve failing validation gates and state the limits of the diagnosis.
