# ASYNCEXECUTION-0014 - Joined Timeout

Status: Accepted
Date: 2026-10-18

## Decision

Add the SC extension `timeout(context, sender, duration)`. It borrows the context, starts work and a relative timer, requests cancellation of the other branch when either completes, and joins both before notifying its receiver. The deadline bounds the cancellation request, not completion latency: running CPU work cannot be preempted. Both branches use the existing execution resources; there is no detached operation or secondary loop.

Use standard provider write_env to give branches separate stop sources, let_value to capture work values, let_error/let_stopped to record terminal outcomes, and unstoppable(when_all(...)) to join without abandoning cleanup. Parent stop requests both branches but cannot cancel the cleanup join. Keep non-template outcome arbitration and cancellation in AsyncExecution.cpp. No new allocation is needed beyond the adapted sender's own allocation policy.

Work and timer start order must tolerate synchronous completions. The first observed work terminal or timer success chooses whether a successful work result or timeout wins. After both branches join, precedence is: failing work Result, failing timer Result, caller stop, deadline winner, work stopped, work values. Thus errors actually reported by native/CPU work retain their identity even after a deadline or stop request. A timer cancelled by work success does not become a timeout. A malformed successful Result on an error channel becomes an unspecified failure. Concurrent ties follow observation order; do not promise wall-clock ordering.

Values are owned decayed snapshots, matching provider let_value storage; references are copied rather than extended as borrowed references. Support zero/multiple values and move-only values with nonthrowing storage construction. Move captured values into the outer operation’s inline storage, sized/aligned from completion signatures; retain them until the outer operation is destroyed. Do not access a completed child operation state. Standard downstream let_value composition retains its received values through asynchronous continuations. Errors must use SC::Result. No exceptions/RTTI or provider-private APIs. The extension requires base sender support, not counting scopes.

The outer operation retains caller-stop callback storage through join and unregisters it before terminal delivery. Defer delivery while a caller-stop callback is requesting both branches, so synchronous completion cannot destroy its sources between requests. Terminal delivery may destroy the outer operation; no state access follows. Preserve forwarding environment queries, including owner scheduler and scope lifetime associations. Buffers, handles and resources remain borrowed until terminal completion; a borrowed pool/context still follows its notification-tail contract (a surrounding joined scope includes those tails).

## Confirmation

Success cancels the timer; deadline joins native cancellation and running CPU work; caller stop joins both; preserve exact work/timer errors across deadline/stop; zero/multiple/move-only values and asynchronous downstream references; synchronous completion/start ordering; controlled concurrent races; terminal self-destruction; environment and scope forwarding; immediate borrowed-buffer reuse after completion. Validate Debug/Release and available platforms, separate compilation/amalgamation and base-provider/scopes-disabled builds. Record platform limitations separately from native conformance.

## Sources

[Official N5050 execution clauses](https://raw.githubusercontent.com/cplusplus/draft/n5050/source/exec.tex): write_env, unstoppable, let_value, let_error, let_stopped and when_all. The timeout policy itself is an SC extension.

## Evidence and limits

Debug/Release dedicated sender suites and normal/failing Base64 and echo demos passed on macOS, Linux and Windows with the pinned stdexec provider. Timeout coverage includes zero/multiple/move-only and alternative-arity values, reference snapshots, downstream asynchronous lifetime, dependent scheduler queries, native receive cancellation followed by buffer/socket reuse, running CPU work joined after deadline/foreign stop, preserved competing Result errors, caller-stop delivery deferral, terminal destruction and enclosing scope forwarding. Test instantiations occupy a separate translation unit to stay within MSVC COFF section limits.

Regenerated single-file builds, enabled/scopes-disabled timeout separate-TU links, enabled/disabled unity builds, dependency/category checks and formatting passed. Native C++26 provider support remains unavailable on these toolchains; this is provider interoperability evidence. Default full SCTest suites remain a final overall-roadmap gate; previous independent watcher/HTTP failures are retained in the evidence log.
