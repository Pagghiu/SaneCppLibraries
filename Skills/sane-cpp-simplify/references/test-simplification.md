# SC simplification review patterns

Use these criteria when consolidating tests, retiring helpers, extracting fixtures, or investigating runtime stalls. They guide candidate review; they do not expand an approved implementation scope.

## Preserve observable behavior while sharing mechanics

For each test scenario, identify the defect it can detect, the operation that reaches it, and the observable assertion. Look for repeated setup, execution and cleanup around distinct scenarios. Share those mechanics while retaining meaningful case names, section selectors and diagnostic context. Compare operations and observable outcomes before and after; assertion counts alone do not establish equivalent coverage.

Trace fixture setup and diagnostics through `TestReport::isTestEnabled` and `quietMode`: a disabled test constructor may still execute. Use registered runtime test names rather than C++ type/file names, and confirm the intended section actually executes. Some SCTest sections launch quiet child copies and treat stdout as protocol data. Preserve selection and output behavior when sharing setup or adding logs; keep unrelated fixture work and diagnostics out of those child streams.

Check whether a test proves the contract it names or only a weaker side effect. For example, an HTTP response body does not prove connection reuse or peer closure. Observe the relevant connection lifecycle separately, and distinguish server-side cleanup from peer EOF. Fixture capacity, implicit defaults and cleanup can accidentally determine the behavior under test; ensure the scenario actually reaches the intended path.

## Evaluate whole-helper replacement costs

Before retiring a test utility, inventory its consumers and less obvious capabilities: raw protocol observations, malformed inputs, staged delivery, timing controls and cleanup. Include replacement code in net savings. Prefer a narrow private consolidation when removing the utility would scatter its responsibilities across callers.

Before sharing similar paths, compare initialization, reset fields, error order, ownership and cleanup. In SC, a Result check may execute work, and caller-owned storage makes lifetime and capacity differences significant. Similar-looking branches can encode different contracts. Record suspected bugs separately; behavior normalization requires its own approval.

Separate correctness repairs from simplification benefits. Charge added assertions and helper declarations to net savings. Stronger coverage may justify more code, but that is a correctness benefit, not a deletion claim.

## Check stored callback lifetime when extracting fixtures

Look for callbacks that detach subscriptions, replace their own stored SC::Function, or destroy the object holding them. Reassignment can destroy the executing lambda; accessing captures afterward can use ended lifetimes. Check cleanup order, reentrancy and the lifetime of referenced fixture objects through the final callback. Defer destructive cleanup until callback execution has finished where required. Existing usage is evidence to inspect, not proof that a pattern is safe to copy.

SC::Event::emit snapshots its listener wrappers, but this does not protect a nested SC::Function stored in the listener's owner; follow ownership through both layers. Before moving a callable out for invocation, check whether detach/unsubscription depends on the stored callable remaining valid. Preserve that condition or explicitly redesign the subscription state.

## Diagnose progress before simplifying synchronization

Distinguish a deadlock from healthy work exceeding a cumulative timeout. Repeated sleeps and scheduler handoffs can accumulate platform-dependent delays. Measure elapsed time and meaningful work checkpoints on the affected platform; do not assume a requested sleep duration is its actual duration. A phase label or polling heartbeat may not prove work is advancing.

Use bounded reproductions and isolated temporary diagnostics when useful. Record the revision, instrumentation changes, controlled parameters and remaining environmental uncertainty. Compile before running, separate runner/bootstrap time from test time, and keep compact patches and logs in the ignored journal.

A passing run with a longer deadline is diagnostic evidence, not a synchronization fix or proof of race freedom. If proposing a no-progress watchdog, advance it only at meaningful checkpoints and verify with bounded fault injection that stalled work still times out. Preserve failing validation gates and state the limits of the diagnosis.

## Verify generated ABI and executed artifacts

Before consolidating a runtime shim, identify the authoritative source and the generators that actually emit it. Header-only and test-runtime outputs can have different owners. Compare compiler/platform guards, symbol signatures, calling conventions and runtime exclusions; textual similarity does not prove ABI equivalence. Keep macro adaptations explicit and account for meaningful contract checks in the estimate. Generated consumers must contain their independent source rather than gain a dependency on the originating library.

For bootstrap/compiler failures, establish which executable ran and whether it contains the current source behavior. Fixtures may copy and future-date a cached bootstrap, preserving an older binary. Retain both stdout and stderr; a wrapper exit code alone can hide the compiler diagnostic. Compare freshness using chronological metadata with appropriate resolution, rather than lexically sorting localized date strings. In controlled old/current comparisons, record selected executable hashes before and after execution so an implicit rebuild cannot silently replace one arm.

When amending instrumentation in an isolated diagnostic snapshot, preserve/update source timestamps or explicitly rebuild the affected owning translation units. Extracted source hashes alone do not establish compiled artifact freshness: epoch-dated archives can leave existing objects newer than amended sources. Verify required runtime markers before drawing conclusions from their presence or absence. Keep setup/build failures and artifact limitations separate from runtime outcomes; correcting the diagnostic setup does not turn a passing run into a repair.
