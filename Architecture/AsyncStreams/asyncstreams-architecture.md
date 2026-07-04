# AsyncStreams Architecture

## Purpose

`AsyncStreams` is the byte-stream composition module. It should let callers read, write, transform, and pipe byte buffers concurrently while keeping memory, back-pressure, and stream topology bounded and explicit.

Future changes should improve stream ergonomics only when they preserve caller-owned buffers and fixed queues. Convenience must not hide storage growth or introduce a concrete `Async` dependency into the core stream module.

## Architectural Shape

`AsyncStreams` separates stream mechanics from async I/O providers. The core module owns buffer views, buffer pools, readable/writable/duplex/transform stream state, events, queues, and `AsyncPipeline`. Request-backed file and socket streams are templates that adapt a compatible event-loop/request type.

Back-pressure is part of the interface: readable streams pause when buffers or request queue space run out, writable streams expose drain/finish state, and pipelines resume sources only when pending writes drain enough to make progress.

## Boundaries

`AsyncStreams` owns byte-buffer movement, bounded queuing, stream state transitions, transform composition, pipeline fan-out, and optional compression transforms. It does not own descriptor creation, event-loop polling, native I/O submission, coroutine tasks, or unbounded storage.

Adapters may bridge to `Async` request types, but core stream code must remain usable without linking the `Async` library.

## Similarities With Other Libraries

Like other SC modules, `AsyncStreams` returns `Result`, uses `Span`, keeps allocation out of library-owned paths, and exposes caller-owned storage requirements. Like `Async`, it models operations as explicit state machines and expects callers to keep referenced objects alive.

## Differences From Other Libraries

Unlike `Async`, it is dependency-free and does not submit work to an OS event facility. Unlike `Await`, it is callback/event style and does not own coroutine frames. Unlike Containers, it does not provide general-purpose data structures; its queues and pools exist to support stream semantics.

## Inspirations

Node.js streams are the documented inspiration for readable, writable, transform, pipeline, and back-pressure concepts. The SC shape keeps the useful stream vocabulary but replaces dynamic buffering and object mode with caller-provided byte buffers, fixed queues, and `Result` failures.

ZLib transforms are inspired by common compression pipeline use cases, but they must remain optional runtime-loaded facilities rather than build dependencies.

## Anti-Inspirations

Do not copy Node.js object mode, dynamic stream graphs, unbounded listener lists, or allocation-backed buffering. Do not turn the module into an `Async` facade. Do not make compression support mandatory for builds that only need plain byte streams.

Inferred negative target: avoid treating `drain` as the only coordination primitive when the fixed buffer pool can provide a simpler resume point after successful writes.

## Architectural Choices

- Keep core `AsyncStreams` dependency-free and adapt request providers through templates.
- Require caller-provided `AsyncBuffersPool` storage and read/write request queues.
- Keep `AsyncPipeline` as one source, bounded transforms, and bounded sinks.
- Use pause/resume and pending-write state to make back-pressure observable.
- Keep ZLib as optional dynamic loading, with load failures reported through `Result`.

## Explicitly Excluded Targets

- Hidden allocation for stream buffers, request queues, listener lists, or pipeline graph nodes.
- Object mode or typed-message streams in the core module.
- Dynamic arbitrary stream graphs in `AsyncPipeline`.
- A mandatory zlib build dependency.
- Direct ownership of event loops, descriptors, coroutine tasks, or thread pools.

## Sources

- [AsyncStreams documentation](../../Documentation/Libraries/AsyncStreams.md)
- [AsyncStreams public interface](../../Libraries/AsyncStreams/AsyncStreams.h)
- [Async request stream adapters](../../Libraries/AsyncStreams/AsyncRequestStreams.h)
- [ZLib API adapter](../../Libraries/AsyncStreams/Internal/ZLibAPI.h)
- [AsyncStreams tests](../../Tests/Libraries/AsyncStreams/AsyncStreamsTest.cpp)
- [ZLib stream tests](../../Tests/Libraries/AsyncStreams/ZLibStreamTest.cpp)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)

## Decision Log

- [ASYNCSTREAMS-0001 - Keep AsyncStreams dependency-free through templated async adapters](asyncstreams-0001-keep-asyncstreams-dependency-free-through-templated-async-adapters.md)
- [ASYNCSTREAMS-0002 - Model back-pressure with caller-owned buffers and fixed queues](asyncstreams-0002-model-back-pressure-with-caller-owned-buffers-and-fixed-queues.md)
- [ASYNCSTREAMS-0003 - Use a fixed-layout pipeline instead of dynamic stream graphs](asyncstreams-0003-use-a-fixed-layout-pipeline-instead-of-dynamic-stream-graphs.md)
- [ASYNCSTREAMS-0004 - Keep ZLib compression as optional runtime loading, not a build dependency](asyncstreams-0004-keep-zlib-compression-as-optional-runtime-loading-not-a-build-dependency.md)
