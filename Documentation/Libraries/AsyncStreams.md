@page library_async_streams Async Streams

@brief 🟨 Concurrently read and write a byte stream staying inside fixed buffers

[TOC]

Async Streams read and write data concurrently from async sources to destinations.

@note Even if the state machine is not strictly depending on @ref library_async, most practical uses of this library will be using it, so it can be considered an extension of @ref library_async

@copydetails group_async_streams

# Features

Async Streams support reading from an async source and placing such reads in a request queue. This queue is bounded, so it will pause the stream when it becomes full.
Data is pushed downstream to listeners of data events.
Such listeners can be for example writers and they will eventually emit a `drain` event that resumes the readable streams that may have been paused.

| Async Stream                                          | Description                           |
|-------------------------------------------------------|---------------------------------------|
| [AsyncReadableStream](@ref SC::AsyncReadableStream)   | @copybrief SC::AsyncReadableStream    | 
| [AsyncWritableStream](@ref SC::AsyncWritableStream)   | @copybrief SC::AsyncWritableStream    |
| [AsyncPipeline](@ref SC::AsyncPipeline)               | @copybrief SC::AsyncPipeline          |

# Implementation

Async streams is heavily inspired by [Node.js streams](https://nodejs.org/api/stream.html) but drops a few features to concentrate on the most useful abstraction.


## Memory allocation
Async streams do not allocate any memory, but use caller provided buffers for handling data and request queues.

# Roadmap

🟩 Usable features:
- Transform Streams

🟦 Complete Features:
- Duplex Streams

💡 Unplanned Features:
- Object Mode
- readable + read mode
