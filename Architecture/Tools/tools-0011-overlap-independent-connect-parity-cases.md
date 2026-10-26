# TOOLS-0011 - Overlap Independent Connect Parity Cases

Status: Accepted
Date: 2026-10-26

## Context

The experimental AsyncExecution parity section exercises eight real refused-connect paths: callbacks,
senders, coroutines and fibers, each with and without cancellation. Windows spends roughly two seconds
waiting for each native refusal, so serial execution costs about16seconds despite little CPU work.

## Decision

Run the non-cancelled callback case first to establish the native error. Publish that immutable result
before starting seven worker threads for the other existing cases. Keep every original collect, cancellation,
dispatch, error comparison and cleanup operation. Each case creates and closes its own event loop/socket,
uses its existing distinct mapped port, and owns its adapter/allocator/stack storage on its worker thread.
Keep watchdog durations unchanged.

Serialize this case's TestCase assertion counters and error reporting with a case-local mutex. Do not
change the global test framework's threading contract. Store worker contexts and threads in caller-owned
arrays, capture only context pointers, and join every started thread before section exit. Adapter operations
and event loops remain on their creating/owning thread. No other tests or global process state are parallelized.

## Validation

Compare serial and parallel versions on the same Windows VM/compiler/configuration and source revision,
including all eight cases and original expectations. Compile before every run. Measure focused and full
execution targets in Debug/Release on macOS, Linux and Windows, then validate the existing hosted matrix.
A faster result is acceptable only with the same ready-before-cancellation semantics and complete cleanup;
thread start/join failures must remain test failures. Preserve first-case error publication and thread-safe
reporting when modifying this fixture later.

## Controlled result

On one Windows VM with identical Release flags, two focused serial runs took24.657/24.437seconds;
two parallel runs took11.735/11.750seconds. This saves approximately12.8seconds per invocation in that
sample, while retaining all eight case bodies byte-for-byte. This includes launcher overhead and is not
an overall CI timing guarantee. Isolated compilation of the two source shapes measured4.469/4.297seconds
serial and4.312/4.297seconds parallel: no meaningful compile-time change, so the gain comes from overlapping
native waits rather than changing optimization coverage or avoiding assertions.
