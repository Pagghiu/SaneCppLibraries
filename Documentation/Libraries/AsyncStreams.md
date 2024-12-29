@page library_async_streams Async Streams

@brief 🟥 Concurrently read and write a byte stream staying inside fixed buffers

[TOC]

Async Streams read and write data concurrently from async sources to destinations.

@note Even if the state machine is not strictly depending on @ref library_async, most practical uses of this library will be using it, so it can be considered an extension of @ref library_async

@copydetails group_async_streams

# Features

This is the list of implemented objects stream types

| Async Stream                                          | Description                           |
|-------------------------------------------------------|---------------------------------------|
| [AsyncReadableStream](@ref SC::AsyncReadableStream)   | @copybrief SC::AsyncReadableStream    | 
| [AsyncWritableStream](@ref SC::AsyncWritableStream)   | @copybrief SC::AsyncWritableStream    |
| [AsyncDuplexStream](@ref SC::AsyncDuplexStream)       | @copybrief SC::AsyncDuplexStream      |
| [AsyncTransformStream](@ref SC::AsyncTransformStream) | @copybrief SC::AsyncTransformStream   |
| [AsyncPipeline](@ref SC::AsyncPipeline)               | @copybrief SC::AsyncPipeline          |
| [ReadableFileStream](@ref SC::ReadableFileStream)     | @copybrief SC::ReadableFileStream     |
| [WritableFileStream](@ref SC::WritableFileStream)     | @copybrief SC::WritableFileStream     |
| [ReadableSocketStream](@ref SC::ReadableSocketStream) | @copybrief SC::ReadableSocketStream   |
| [WritableSocketStream](@ref SC::WritableSocketStream) | @copybrief SC::WritableSocketStream   |


# Status
🟥 Draft  

Async Streams are for now in Draft state.
It's also possible that its API will evolve a little bit to be less verbose and there is also lack of nice examples, aside from the tests.

# Implementation

Async Streams support reading from an async source and placing such reads in a bounded request queue that will pause the stream when it becomes full or when there are no available buffers.
Data is pushed downstream to listeners of data events, that are either transform streams or writers streams.
Writers will eventually emit a `drain` event to signal that they can write more data. 
Such event can be used to resume the readable streams that may have been paused.
AsyncPipeline doesn't use the `drain` event but it just resumes readable streams after every successful write.
This works because the Readable will pause when running out of buffers, allowing them to resume when a new one is made available.

## Memory allocation
Async streams do not allocate any memory, but use caller provided buffers for handling data and request queues.

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.28 - C++ Async Readable Streams](https://www.youtube.com/watch?v=MFPjoOUTlBo)
- [Ep.29 - C++ Async Writable Streams](https://www.youtube.com/watch?v=0OXLxIDvmOU)
- [Ep.30 - C++ Async Streams Pipeline](https://www.youtube.com/watch?v=8rYQ2ApxnwA)
- [Ep.31 - C++ Async Socket Streams](https://www.youtube.com/watch?v=0x6TLV_ig-A)
- [Ep.32 - C++ Async Transform Streams - Part 1](https://www.youtube.com/watch?v=Ul7DdQGrETo)

# Blog

Some relevant blog posts are:

- [November 2024 Update](https://pagghiu.github.io/site/blog/2024-11-30-SaneCppLibrariesUpdate.html)

# Roadmap

🟨 MVP features
- Transform Streams

🟩 Usable features:
- Pipeline pause
- Pipeline resume

🟦 Complete Features:
- writev style asyncWrite

💡 Unplanned Features:
- Object Mode
- readable + read mode
