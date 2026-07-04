# Async Architecture

## Purpose

`Async` is the low-level asynchronous I/O module for Sane C++ Libraries. It should expose completion-based files, sockets, timers, process exits, signals, filesystem operations, background work, wake-ups, file readiness, and external completions while preserving visible memory ownership and platform control.

Future changes should treat `AsyncEventLoop` plus caller-owned `AsyncRequest` objects as the primary interface. Higher-level coroutine or stream shapes belong in `Await`, `AsyncStreams`, or another adapter unless they need a new low-level OS completion primitive.

## Architectural Shape

`Async` is a deep module over native event facilities. The public interface is a caller-owned event loop, request objects, result callbacks, and explicit run modes; the implementation hides IOCP, kqueue, io_uring, epoll, pidfd, wake-up, and cancellation details behind internal code.

Every request object is both the public handle and the in-flight storage. Requests move through a state machine owned by the event loop, may be sequenced, may be stopped, and may be reactivated from callbacks. The event loop must not allocate hidden request state.

## Boundaries

`Async` owns event-loop submission, polling, dispatch, request state, cancellation, native backend selection, and completion delivery. It depends on descriptor-owning libraries such as File, FileSystem, Socket, and Threading where the operation requires their types.

`Async` does not own byte-stream composition, coroutine scheduling, protocol buffering, descriptor creation policies outside its async helpers, or dynamic storage pools. Those concerns must remain in callers or higher-level modules.

## Similarities With Other Libraries

Like the rest of SC, `Async` returns `Result`, keeps allocation explicit, avoids exceptions, keeps public headers free of OS headers, and uses caller-provided storage. Like File, Socket, and Threading, it is a platform abstraction, not a portability fantasy: unsupported backend behavior should be visible through options, errors, or documentation.

## Differences From Other Libraries

Unlike many SC libraries, `Async` intentionally has several internal platform backends and a larger lifecycle contract. Unlike `AsyncStreams`, it owns OS completion submission rather than stream back-pressure. Unlike `Await`, it is callback-first and does not allocate coroutine frames. Unlike Foundation/Common, it is not a primitive facade or source-sharing area; it is an operational runtime module.

## Inspirations

Use the native platform model as the implementation inspiration: IOCP on Windows, kqueue on macOS, direct io_uring on Linux when available, and epoll fallback. The split submit/poll/dispatch interface is inspired by GUI and foreign event-loop integration requirements documented in the Async run-mode docs.

Some other inspirations include:
- [libuv](https://github.com/libuv/libuv) - (Even if it's designed more around readiness rather than completion) as it contains a lot of useful accumulated knowledge around cross-platform async/io.
- [A Programmer-Friendly I/O Abstraction Over io_uring and kqueue](https://tigerbeetle.com/blog/2022-11-23-a-friendly-abstraction-over-iouring-and-kqueue/) - Very nice article on completion centric abstraction
- [libxev](https://github.com/mitchellh/libxev) - It's looks inspired by the above article


## Anti-Inspirations

Do not turn `Async` into an allocating task scheduler, a promise/future framework, a copy Boost.Asio, or C++ 26 Executors or a generic stream library. Do not hide thread-pool use behind automatic allocation. Do not merge readiness watching and external completion injection just because both pass through the event loop.

Inferred negative target: avoid APIs that make request lifetime look owned by the event loop when the storage is still caller-owned.

## Architectural Choices

- Keep request storage caller-owned and memory-stable until completion, stop, or free state.
- Keep `run`, `runOnce`, and `runNoWait` as convenience modes over submit, poll, and dispatch phases.
- Prefer native backends and make thread-pool fallback explicit through caller-provided `ThreadPool` state.
- Use explicit request reactivation for recurring work instead of hidden repeating wrappers.
- Keep file readiness and external completion as separate request concepts.

## Explicitly Excluded Targets

- Hidden heap allocation of request or completion state.
- A cross-platform guarantee that every descriptor behaves identically on every backend.
- Automatic background worker creation for operations that can block.
- Coroutine, stream, or protocol-level ownership abstractions in the low-level event loop.
- Public exposure of OS headers or backend handles except through deliberate opaque types or platform-specific escape hatches.

## Sources

- [Async documentation](../../Documentation/Libraries/Async.md)
- [Async public interface](../../Libraries/Async/Async.h)
- [Async implementation](../../Libraries/Async/Async.cpp)
- [Async contract tests](../../Tests/Libraries/Async/AsyncContractTest.cpp)
- [Project principles](../../Documentation/Pages/Principles.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)

## Decision Log

- [ASYNC-0001 - Keep AsyncRequest objects caller-owned and memory-stable](async-0001-keep-asyncrequest-objects-caller-owned-and-memory-stable.md)
- [ASYNC-0002 - Split event-loop running into submit, poll, and dispatch phases](async-0002-split-event-loop-running-into-submit-poll-and-dispatch-phases.md)
- [ASYNC-0003 - Prefer native backend I/O with explicit thread-pool escape hatches](async-0003-prefer-native-backend-io-with-explicit-thread-pool-escape-hatches.md)
- [ASYNC-0004 - Separate file readiness from external completion injection](async-0004-separate-file-readiness-from-external-completion-injection.md)
- [ASYNC-0005 - Use request reactivation for recurring async work](async-0005-use-request-reactivation-for-recurring-async-work.md)
