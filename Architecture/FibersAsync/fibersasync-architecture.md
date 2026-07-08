# FibersAsync Architecture

## Purpose

`FibersAsync` is the bridge that lets stackful fiber tasks perform `Async` I/O with synchronous-looking code. It should
preserve `AsyncEventLoop` ownership, `Fibers` scheduling semantics, no hidden allocation, caller-owned buffers, and
plain `Result` error propagation.

Future changes should make I/O from worker fibers convenient without merging the CPU scheduler and async I/O event loop
into one implicit runtime.

## Architectural Shape

`FiberAsyncIO` wraps an externally owned `FiberScheduler&` and an externally owned `AsyncEventLoop&`. The async event
loop remains thread-affine. Calls from the owner thread may start operations directly; calls from worker fibers on other
threads post bounded commands to the owner thread and then suspend the fiber on fiber synchronization primitives until
the async operation completes, fails, or is canceled.

Each operation keeps request state explicit and returns extra values through small result objects, such as
`FiberAsyncSocketReceiveResult` or `FiberAsyncFileReadResult`. The received or read data remains in caller-provided
buffers.

## Boundaries

`FibersAsync` owns the bridge between fiber suspension and `AsyncRequest` completion, owner-thread command posting,
operation counters, cooperative cancellation plumbing, and fiber-friendly wrappers for selected async operations.

`FibersAsync` does not own the `AsyncEventLoop`, the `FiberScheduler`, sockets, files, buffers, protocols, coroutine
tasks, worker threads, or a backend-selection facade. Those remain in callers, `Async`, `Fibers`, or later explicit
adapters.

## Similarities With Other Libraries

Like `Await`, `FibersAsync` wraps an existing `AsyncEventLoop` instead of replacing it. Like `Async`, it preserves
caller-owned descriptors, buffers, request semantics, and result objects. Like `Fibers`, it suspends cooperatively and
uses bounded caller-provided storage for cross-thread command posting.

## Differences From Other Libraries

Unlike `Await`, `FibersAsync` does not allocate coroutine frames and does not require C++20 coroutines. Unlike `Async`, it
offers synchronous-looking control flow by suspending a fiber instead of returning to a callback. Unlike `Fibers`, it is
not a general CPU scheduler and intentionally depends on `Async`.

## Inspirations

The bridge follows the same migration goal as `Await`: let users keep lower-level async ownership visible while writing
linear-looking code. It also follows fiber-friendly I/O designs where blocking-looking calls suspend the current fiber
rather than blocking the OS worker thread.

## Anti-Inspirations

Do not add a hidden async event-loop thread, hidden command queue allocation, implicit descriptor ownership,
promise/future handles, exceptions for I/O failure, or an abstract I/O facade before at least two real backends justify
one.

## Architectural Choices

- Wrap, do not own, both `FiberScheduler` and `AsyncEventLoop`.
- Keep `AsyncEventLoop` thread-affine and use bounded command posting for cross-thread starts/stops.
- Return plain `Result`; operation-specific output belongs in caller-provided result objects.
- Keep buffers caller-owned and report data spans as views into those buffers.
- Treat cancellation as cooperative and completion-aware.
- Keep runtime-selectable I/O backends out of the first stable shape.
- Defer `AwaitTask` and fiber-task adapters until both runtimes have stable real-use bridge requirements.

## Explicitly Excluded Targets

- A combined `Fibers` plus `Async` mega-runtime.
- Hidden owner threads or hidden worker pools.
- Unbounded cross-thread command queues.
- Direct `AwaitTask` bridging before both runtimes have stable real-use requirements.
- Runtime-selectable blocking/threaded/native I/O backends in the current API.

## Sources

- [FibersAsync documentation](../../Documentation/Libraries/FibersAsync.md)
- [FibersAsync public interface](../../Libraries/FibersAsync/FibersAsync.h)
- [FibersAsync implementation](../../Libraries/FibersAsync/FibersAsync.cpp)
- [FibersAsync tests](../../Tests/Libraries/FibersAsync/FibersAsyncTest.cpp)
- [Fibers architecture](../Fibers/fibers-architecture.md)
- [Async architecture](../Async/async-architecture.md)
- [Await architecture](../Await/await-architecture.md)
- [Fibers scale-up plan](../../Documentation/Plans/FibersPlan.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0006 - Use explicit result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)

## Decision Log

- [FIBERSASYNC-0001 - Keep FibersAsync as the bridge from Fibers to Async](fibersasync-0001-keep-fiberasync-as-the-bridge-from-fibers-to-async.md)
- [FIBERSASYNC-0002 - Keep AsyncEventLoop owner-thread affine with bounded command posting](fibersasync-0002-keep-asynceventloop-owner-thread-affine-with-bounded-command-posting.md)
- [FIBERSASYNC-0003 - Return Result and operation-specific caller-owned output objects](fibersasync-0003-return-result-and-operation-specific-caller-owned-output-objects.md)
- [FIBERSASYNC-0004 - Keep FiberAsyncIO concrete until a second backend exists](fibersasync-0004-keep-fiberasyncio-concrete-until-a-second-backend-exists.md)
- [FIBERSASYNC-0005 - Defer AwaitTask fiber adapters until both runtimes stabilize](fibersasync-0005-defer-awaittask-fiber-adapters-until-both-runtimes-stabilize.md)
- [FIBERSASYNC-0006 - Keep cancellation completion-aware across async stop races](fibersasync-0006-keep-cancellation-completion-aware-across-async-stop-races.md)
