# Await Architecture

## Purpose

`Await` is the C++20 coroutine layer over `Async`. It should let coroutine bodies express async control flow with `co_await` while preserving SC rules: plain `Result`, caller-owned outputs, explicit memory, visible cancellation, and compatibility with callback-style `Async`.

Future changes should reduce callback boilerplate only when they keep `Async` lifetime semantics visible. `Await` is not a replacement runtime for `AsyncEventLoop`.

## Architectural Shape

`AwaitEventLoop` wraps an existing `AsyncEventLoop&` and an opened `AwaitAllocator`. Coroutine functions return `AwaitTask`; awaiters embed or own the lower-level `AsyncRequest` they suspend on; coroutine frames are allocated by the caller-selected allocator.

Helpers on `AwaitEventLoop` should be thin awaiter factories for one `Async` operation or small no-allocation loops over caller storage. Structured concurrency lives in caller-owned `AwaitTaskGroup` and fixed-slot `AwaitTaskRegistry` storage.

## Boundaries

`Await` owns coroutine frame allocation policy, coroutine task state, awaiter wrappers, cooperative cancellation hooks, task groups, task registries, and coroutine-friendly wrappers for selected `Async` operations.

`Await` does not own the underlying event loop, descriptors, buffers, long-lived watcher streams, protocol parsers, hidden task schedulers, or heap-backed task registries. File and filesystem awaiters that need blocking work require caller-provided `ThreadPool` storage.

## Similarities With Other Libraries

Like `Async`, `Await` keeps sockets, descriptors, buffers, result objects, and task objects stable while operations are active. Like the rest of SC, it returns `Result`, avoids exceptions for control flow, and keeps allocation policy visible. Like allocation-capable modules, it makes non-fixed allocation modes explicit opt-ins.

## Differences From Other Libraries

Unlike `Async`, `Await` uses C++20 coroutine machinery and therefore must own coroutine frame allocation. Unlike `AsyncStreams`, it is operation/task oriented rather than byte-stream oriented. Unlike Memory, it is not a general allocator library; `AwaitAllocator` exists only to make coroutine frames explicit.

## Inspirations

The primary inspiration is `Async`: every awaiter should preserve the underlying request shape and platform behavior. Python `asyncio.TaskGroup` is an evidenced inspiration for task-group control flow, but SC keeps task objects, pointer arrays, result arrays, buffers, and allocator storage caller-owned.

## Anti-Inspirations

Do not build an implicit coroutine runtime, hidden scheduler, RAII shutdown guard that can hide failing drains, heap-backed detached task registry, or direct watcher-stream method on `AwaitEventLoop`.

Inferred negative target: avoid promise/future APIs where task start, allocation, cancellation, and result observation are hidden behind shared ownership.

## Architectural Choices

- Wrap, do not own, `AsyncEventLoop`.
- Require `AwaitAllocator`; fixed caller-owned storage is the preferred path.
- Keep direct `AwaitEventLoop` helpers thin and move stateful adapters into explicit helper objects.
- Store task groups and registries in caller-provided spans.
- Treat cancellation as cooperative, result-based, and completion-wins when the underlying operation already completed.

## Explicitly Excluded Targets

- Hidden standard allocation fallback for coroutine frames.
- Exceptions as coroutine error propagation.
- Direct `AwaitEventLoop` methods for long-lived watcher/channel streams without explicit bounded storage.
- Heap-owned background task schedulers.
- Full stable status before lifecycle, helper-shape, and no-stdlib coroutine follow-ups are resolved.

## Sources

- [Await documentation](../../Documentation/Libraries/Await.md)
- [Await public interface](../../Libraries/Await/Await.h)
- [Await implementation](../../Libraries/Await/Await.cpp)
- [Await tests](../../Tests/Libraries/Await/AwaitTest.cpp)
- [Async documentation](../../Documentation/Libraries/Async.md)
- [Project principles](../../Documentation/Pages/Principles.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0006 - Use explicit result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)

## Decision Log

- [AWAIT-0001 - Wrap caller-owned AsyncEventLoop instead of owning a coroutine runtime](await-0001-wrap-caller-owned-asynceventloop-instead-of-owning-a-coroutine-runtime.md)
- [AWAIT-0002 - Require explicit coroutine frame allocation](await-0002-require-explicit-coroutine-frame-allocation.md)
- [AWAIT-0003 - Keep Await helpers thin and keep long-lived streams out of AwaitEventLoop](await-0003-keep-await-helpers-thin-and-keep-long-lived-streams-out-of-awaiteventloop.md)
- [AWAIT-0004 - Use caller-owned TaskGroup and TaskRegistry storage](await-0004-use-caller-owned-taskgroup-and-taskregistry-storage.md)
- [AWAIT-0005 - Keep cancellation cooperative and Result-based](await-0005-keep-cancellation-cooperative-and-result-based.md)
