@page library_async_streams Async Streams

@brief 🟨 Concurrently read, write and transform byte streams

[TOC]

[SaneCppAsyncStreams.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAsyncStreams.h) is a library that reads and writes data concurrently from async sources to destinations.

# Dependencies
- Dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

![Dependency Graph](AsyncStreams.svg)


# Features

This is the list of implemented objects stream types

| Async Stream                                                      | Description                               |
|-------------------------------------------------------------------|-------------------------------------------|
| [AsyncReadableStream](@ref SC::AsyncReadableStream)               | @copybrief SC::AsyncReadableStream        | 
| [AsyncWritableStream](@ref SC::AsyncWritableStream)               | @copybrief SC::AsyncWritableStream        |
| [AsyncDuplexStream](@ref SC::AsyncDuplexStream)                   | @copybrief SC::AsyncDuplexStream          |
| [AsyncTransformStream](@ref SC::AsyncTransformStream)             | @copybrief SC::AsyncTransformStream       |
| [AsyncPipeline](@ref SC::AsyncPipeline)                           | @copybrief SC::AsyncPipeline              |
| [AsyncReadableFileStream](@ref SC::AsyncReadableFileStream)       | @copybrief SC::AsyncReadableFileStream    |
| [AsyncWritableFileStream](@ref SC::AsyncWritableFileStream)       | @copybrief SC::AsyncWritableFileStream    |
| [AsyncReadableSocketStream](@ref SC::AsyncReadableSocketStream)   | @copybrief SC::AsyncReadableSocketStream  |
| [AsyncWritableSocketStream](@ref SC::AsyncWritableSocketStream)   | @copybrief SC::AsyncWritableSocketStream  |

# Details

@copydetails group_async_streams

# Status
🟨 MVP  

A basic set of stream to pipe Sockets and Files (Pipes included) has been implemented.  
Also basic transform streams (ZLib based) have been implemented.  
It's possible that its API will evolve a little bit to be less verbose and there is also lack of nice examples, aside from the tests.

# Implementation

Async Streams support reading from an async source and placing such reads in a bounded request queue that will pause the stream when it becomes full or when there are no available buffers.
Data is pushed downstream to listeners of data events, that are either transform streams or writers streams.
Writers will eventually emit a `drain` event to signal that they can write more data. 
Such event can be used to resume the readable streams that may have been paused.
AsyncPipeline doesn't use the `drain` event but it just resumes readable streams after every successful write.
This works because the Readable will pause when running out of buffers, allowing them to resume when a new one is made available.

ReadableFileStream and WritableFileStream now also support initialization directly from SC::PipeDescriptor (in addition to SC::FileDescriptor).
This makes named pipes (see [File](@ref library_file)) compose directly with Async Streams without manual extraction of `readPipe` / `writePipe`.

## Memory allocation
Async streams do not allocate any memory, but use caller provided buffers for handling data and request queues.

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
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 783			| 437		| 1220	|
| Sources   | 1839			| 275		| 2114	|
| Sum       | 2622			| 712		| 3334	|
