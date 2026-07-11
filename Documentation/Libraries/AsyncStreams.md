@page library_async_streams Async Streams

@brief 🟨 Bounded byte-stream pipelines with explicit backpressure

[TOC]

[SaneCppAsyncStreams.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAsyncStreams.h)
provides allocation-free readable, writable, transform, and pipeline state machines for moving bytes between
asynchronous producers and consumers.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](AsyncStreams.svg)


# What Async Streams Is For

Async Streams is for workloads where data arrives or leaves over time and should be processed without first collecting
the entire payload in memory. A source can read a file or socket into a bounded pool of buffers, transforms can process
those buffers incrementally, and one or more sinks can consume them concurrently.

The core library defines the stream and pipeline state machines without depending on [Async](@ref library_async), File,
or Socket. `AsyncRequestStreams.h` supplies templates that adapt compatible asynchronous request and event-loop types;
the file and socket compositions in this repository instantiate them with [Async](@ref library_async).

Use Async Streams when you need:

- incremental byte processing over asynchronous sources and destinations;
- bounded memory usage selected by the caller;
- backpressure when a sink, transform, or buffer pool cannot keep up;
- a fixed pipeline of one source, up to eight transforms, and up to eight sinks;
- file, pipe, socket, or compression streams built on the same byte-buffer model.

It is not a general container for arbitrary objects, a pull-based `read()` interface, or an unbounded buffering layer.
There is currently no object mode, and consumers receive pushed data events rather than a Node.js-style readable event.

# Mental Model

An Async Streams graph has four layers:

1. `AsyncBuffersPool` names and tracks caller-provided `AsyncBufferView` objects.
2. `AsyncReadableStream` produces buffer IDs; `AsyncWritableStream` consumes them.
3. `AsyncDuplexStream` combines both directions, while `AsyncTransformStream` consumes input and produces transformed
   output.
4. `AsyncPipeline` connects one source through an optional transform chain to one or more sinks.

Streams exchange buffer IDs instead of owning byte allocations. The pool resolves an ID to its readable or writable
span and reference-counts that view while it is in flight. A reusable view returns to the pool when its references reach
zero. Child views can expose a slice without copying while retaining the parent buffer.

Read and write request queues are also supplied by the caller. The internal circular queues reserve one slot to
distinguish full from empty, so an array of `N + 1` requests provides `N` usable queued requests.

# Supplying A Bounded Buffer Pool

This compiled test divides one caller-owned byte allocation into two reusable views. Real applications choose both the
number and size of buffers from their expected concurrency and chunk size.

@snippet Tests/Libraries/AsyncStreams/AsyncRequestStreamsTest.cpp AsyncStreamsBufferPoolSnippet

The pool never grows. When a readable cannot obtain a sufficiently large free buffer, `getBufferOrPause()` pauses it.
When downstream processing releases a buffer, the pipeline can resume the source. This makes memory exhaustion part of
the flow-control model instead of an implicit allocation.

# A File-To-File Pipeline

The ready-made request streams adapt file and socket descriptors to the core stream model. After constructing the
caller-owned queues and associating descriptors with an event loop, a basic file copy is a source connected directly to
a sink:

@snippet Tests/Libraries/AsyncStreams/AsyncRequestStreamsTest.cpp AsyncStreamsFilePipelineSnippet

The source, every transform, and every sink in one pipeline must use the same `AsyncBuffersPool`. `pipe()` validates the
layout and registers event listeners; `start()` begins source production. Completion remains asynchronous: the event
loop must continue running until the streams finish or are destroyed.

`AsyncReadableFileStream` and `AsyncWritableFileStream` accept `FileDescriptor` and `PipeDescriptor`, so anonymous and
named pipes from [File](@ref library_file) compose without manually extracting their read or write endpoint.

# Backpressure And Buffer Lifetime

Backpressure can come from either side of the graph:

- a readable pauses when its read queue is full or no reusable buffer is available;
- a writable rejects a new write with an invalid `Result` when its bounded write queue is full;
- a transform may retain input while waiting for output space or asynchronous work;
- a pipeline records bounded pending writes and resumes the relevant readable as writes complete.

Readable implementations acquire a buffer, fill it, call `push(bufferID, size)`, and release their own reference. The
stream retains the buffer for queued delivery. Writable implementations finish each accepted operation through
`finishedWriting()`, which advances the queue and releases the stream's reference. Custom streams must respect this
protocol; releasing a view too early invalidates downstream data, while failing to finish an operation prevents drain
and end propagation.

`eventData` listeners should treat the buffer as borrowed for the callback unless they explicitly add a reference.
Buffers can be read by multiple sinks because the pool tracks references, but writable access is only meaningful where
the underlying view permits it.

# Transforms And Fan-Out

An `AsyncTransformStream` is a duplex stream: it consumes buffers through its writable side and emits new buffers from
its readable side. `SyncZLibTransformStream` runs compression work synchronously; `AsyncZLibTransformStreamT` schedules
the same work through a compatible event loop and can be directed to a caller-selected thread pool.

A pipeline has a fixed layout rather than a dynamically growing graph. It supports at most eight transforms and eight
sinks. Every sink receives the source data, with buffer references held until each write completes. This is useful for
bounded fan-out, but the slowest sink applies backpressure to the shared source. If consumers need independent pacing or
unbounded history, that storage and policy belongs outside this library.

# Errors, Ending, And Destruction

Stream errors are delivered through `eventError`, and `AsyncPipeline::eventError` forwards failures from connected
sources, transforms, and sinks. `pushEnd()` ends a readable after queued data is delivered; `end()` lets a writable
finish queued and in-flight writes before emitting its finish event.

`destroy()` is the forceful, idempotent path for stopping a stream and its pending asynchronous work. Custom
implementations that override asynchronous destruction must eventually call the corresponding
`finishedDestroyingReadable()` or `finishedDestroyingWritable()` hook. Request-stream adapters do not close their file
or socket descriptor by default; call `setAutoCloseDescriptor(true)` only when the stream owns that descriptor. Streams
auto-destroy after normal completion by default, and this can be disabled when the surrounding owner needs a different
lifetime policy.

# Allocation Model

Async Streams performs no hidden dynamic allocation. The caller supplies:

- the byte memory and `AsyncBufferView` slots in each `AsyncBuffersPool`;
- readable and writable request-queue arrays;
- stream, transform, pipeline, and event-loop objects;
- any storage or thread pools required by the underlying asynchronous operations.

Some examples use `Buffer`, `String`, or `Vector` to conveniently own test memory. Those allocations belong to the
caller and are not required by the stream machinery; fixed spans can be used instead. Capacity failures are observable
as pauses or invalid `Result` values and must be handled as normal control flow.

# Choosing The Neighboring Abstraction

- Use [Async](@ref library_async) directly for individual file, socket, process, timer, or event-loop operations where a
  stream graph adds no value.
- Use Async Streams when the unit of work is a sequence of byte chunks and backpressure must connect producer speed to
  consumer capacity.
- Use [HttpClient](@ref library_http_client) for HTTP requests. Its optional async adapter exposes body streams using
  this model without making the poll-driven core depend on Async Streams.
- Use [Http](@ref library_http) for the higher-level asynchronous HTTP server and WebSocket compositions already built
  on these streams.

# Details

For the complete API reference, see @ref group_async_streams. The tests under `Tests/Libraries/AsyncStreams` contain
working file, pipe, socket, fan-out, backpressure, child-view, and synchronous/asynchronous zlib compositions.

# Status
🟨 MVP

The core file, pipe, socket, pipeline, and zlib transform paths are implemented and exercised by tests. The pipeline
automatically pauses and resumes streams as part of backpressure handling, but it does not yet expose explicit
pipeline-level pause, resume, or cancellation controls. The API remains an MVP: some setup is verbose, and examples
outside the test suite are limited.

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.28 - C++ Async Readable Streams](https://www.youtube.com/watch?v=MFPjoOUTlBo)
- [Ep.29 - C++ Async Writable Streams](https://www.youtube.com/watch?v=0OXLxIDvmOU)
- [Ep.30 - C++ Async Streams Pipeline](https://www.youtube.com/watch?v=8rYQ2ApxnwA)
- [Ep.31 - C++ Async Socket Streams](https://www.youtube.com/watch?v=0x6TLV_ig-A)
- [Ep.32 - C++ Async Transform Streams - Part 1](https://www.youtube.com/watch?v=Ul7DdQGrETo)
- [Ep.33 - C++ Async Transform Streams - Part 2](https://www.youtube.com/watch?v=KKwohFmAUCk)
- [Ep.34 - C++ Async Transform Streams - Part 3](https://www.youtube.com/watch?v=vCh6vEfiISI)

# Blog

Some relevant blog posts are:

- [November 2024 Update](https://pagghiu.github.io/site/blog/2024-11-30-SaneCppLibrariesUpdate.html)
- [December 2024 Update](https://pagghiu.github.io/site/blog/2024-12-31-SaneCppLibrariesUpdate.html)
- [January 2025 Update](https://pagghiu.github.io/site/blog/2025-01-31-SaneCppLibrariesUpdate.html)
- [August 2025 Update](https://pagghiu.github.io/site/blog/2025-08-31-SaneCppLibrariesUpdate.html)
- [November 2025 Update](https://pagghiu.github.io/site/blog/2025-11-30-SaneCppLibrariesUpdate.html)
- [December 2025 Update](https://pagghiu.github.io/site/blog/2025-12-31-SaneCppLibrariesUpdate.html)
- [January 2026 Update](https://pagghiu.github.io/site/blog/2026-01-31-SaneCppLibrariesUpdate.html)
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟩 Usable features:
- Pipeline pause
- Pipeline resume

🟦 Complete Features:
- writev style asyncWrite

💡 Unplanned Features:
- Object Mode
- readable + read mode

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/AsyncStreams`.
Single File counts
`SaneCppAsyncStreams.h`.
Standalone counts `SaneCppAsyncStreamsStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 924		| 1798		| 2722	|
| Single File | 1710		| 2045		| 3755	|
| Standalone  | 1710		| 2045		| 3755	|
