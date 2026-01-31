@page library_http Http

@brief 🟥 HTTP parser and server

[TOC]

[SaneCppHttp.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttp.h) is a library implementing a hand-written http 1.1 parser, and server.

# Dependencies
- Dependencies: [AsyncStreams](@ref library_async_streams)
- All dependencies: [Async](@ref library_async), [AsyncStreams](@ref library_async_streams), [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](Http.svg)


# Features
- HTTP 1.1 Parser
- HTTP 1.1 Server

# Status
🟥 Draft  
In current state the library is able to host simple static website but it cannot be used for any internet facing application.  
Additionally its API will be changing heavily as it's undergoing major re-design to make it fully allocation free and extend it to support more of the HTTP 1.1 standard.

# Description
The HTTP parser is an incremental parser, that will emit events as soon as a valid element has been successfully parsed.
This allows handling incomplete responses without needing holding it entirely in memory.

The HTTP server is for now just a basic implementations and it's missing many important features.
It's however capable of serving files through http while staying inside a few user provided fixed buffers, without allocating any kind of dynamic memory.

The HTTP client has been for now moved to the tests as it needs significant re-work to become useful.  

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.27 - C++ Async Http Web Server](https://www.youtube.com/watch?v=yg438A9Db50)

# Blog

Some relevant blog posts are:

- [August 2024 Update](https://pagghiu.github.io/site/blog/2024-08-30-SaneCppLibrariesUpdate.html)
- [September 2025 Update](https://pagghiu.github.io/site/blog/2025-09-30-SaneCppLibrariesUpdate.html)
- [November 2025 Update](https://pagghiu.github.io/site/blog/2025-11-30-SaneCppLibrariesUpdate.html)
- [December 2025 Update](https://pagghiu.github.io/site/blog/2025-12-31-SaneCppLibrariesUpdate.html)
- [January 2026 Update](https://pagghiu.github.io/site/blog/2026-01-31-SaneCppLibrariesUpdate.html)

## HttpAsyncServer
@copydoc SC::HttpAsyncServer

## HttpAsyncFileServer
@copydoc SC::HttpAsyncFileServer

# Examples

- [SCExample](@ref page_examples) features the `WebServerExample` sample showing how to use SC::HttpAsyncFileServer and SC::HttpAsyncServer
- Unit tests show how to use SC::HttpAsyncFileServer and SC::HttpAsyncServer

# Roadmap

🟨 MVP
- Implement Stream based client and agent for socket re-use and connection persistence
- Support mostly used HTTP verbs / methods
- HTTP 1.1 Chunked Encoding

🟩 Usable Features:
- Implement ([Web Streams API](https://developer.mozilla.org/en-US/docs/Web/API/Streams_API))
- Connection Upgrade
- Multipart encoding

🟦 Complete Features:
- HTTPS
- Support all HTTP verbs / methods

💡 Unplanned Features:
- Http 2.0 
- Http 3.0

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 446			| 321		| 767	|
| Sources   | 2557			| 473		| 3030	|
| Sum       | 3003			| 794		| 3797	|
