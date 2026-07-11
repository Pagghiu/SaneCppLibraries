@page library_file File

@brief 🟩 Synchronous Disk File I/O

[SaneCppFile.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFile.h) is a library implementing synchronous I/O operations on files and pipes.  

[TOC]

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](File.svg)


# When to use File

Use File when the program already knows **which byte stream** it wants to work with and needs portable,
synchronous descriptor operations:

- opening a file and reading, writing, seeking or resizing it;
- inspecting or changing metadata through an already-open descriptor;
- redirecting a child process through an anonymous pipe;
- connecting two processes through a named pipe; or
- duplicating standard input, output or error without taking ownership of the process-wide handle.

File deliberately stops at the descriptor boundary. Use [File System](@ref library_file_system) to create directories,
copy or rename paths, traverse directory contents, or ask questions about a path that has not been opened. Use
[Async](@ref library_async) when I/O must be driven by an event loop instead of blocking the calling thread; it accepts
the same SC::FileDescriptor handles, including the read and write endpoints inside SC::PipeDescriptor.

# A descriptor is an owned byte stream

SC::FileDescriptor is the central abstraction. It owns one native file handle on Windows or file descriptor on POSIX,
but presents one API for regular files, pipe endpoints and duplicated standard streams. It is move-only: destruction or
SC::FileDescriptor::close releases the native handle, while moving transfers that responsibility. This makes descriptor
ownership explicit and prevents two objects from accidentally closing the same handle.

File itself does not allocate. Reads write into a caller-provided SC::Span and report the portion actually filled; writes
consume a caller-provided span (or SC::StringSpan through `writeString`). The `readUntilEOF` convenience delegates any
growth and allocation to its caller-provided buffer. Errors are returned as SC::Result.

`open` requires an absolute path in a supported encoding, and the converted native path must fit SC::StringPath. This
tested excerpt shows the core write-close-reopen-read sequence:

\snippet Tests/Libraries/File/FileTest.cpp FileSnippet

The open mode describes both access and creation behavior. SC::FileOpen::Read and SC::FileOpen::ReadWrite require an
existing file; append modes create when needed and preserve existing contents; write modes create or truncate. Additional
options control exclusivity, inheritance by child processes, blocking behavior and cache-bypassing synchronous opens.

## Reads, EOF and positions

A successful SC::FileDescriptor::read can return fewer bytes than requested. Its output span identifies the bytes that
were actually read, and an empty output span signals EOF. Choose the operation according to the framing you already know:

- SC::FileDescriptor::read performs one read from the current position, or from an explicit offset;
- SC::FileDescriptor::readUntilFullOrEOF repeats reads until the supplied span is full or EOF is reached;
- SC::FileDescriptor::readUntilEOF grows a caller-supplied `String`, `SmallString` or `Buffer` until a stream ends.

The last form also works for non-seekable streams such as pipes and duplicated standard handles. It resets and resizes the
supplied growable buffer as data arrives, so allocation behavior comes from that buffer type: inline capacity can avoid an
allocation for small inputs, while growth beyond available capacity may allocate or fail according to the type's policy.

Regular files additionally support seeking relative to the start, end or current position. On POSIX, offset-based reads
and writes use positional I/O and preserve the descriptor position; on Windows, they seek and then perform ordinary I/O.
Portable code must therefore not assume that these overloads preserve position or provide independent concurrent access.

## Metadata, durability and resizing

SC::FileDescriptor::stat queries the entry behind the open handle, avoiding a second path lookup. The portable portion of
SC::FileDescriptorStat describes entry type, size, link count and timestamps; platform-specific substructures retain the
native POSIX identity/mode fields or Windows attributes and file identity.

Descriptor-bound SC::FileDescriptor::chmod and SC::FileDescriptor::chown target that same open entry on POSIX. On Windows,
`chmod` maps only the owner-write bit to the read-only attribute, and `chown` currently validates the descriptor but does
not change ownership. SC::FileDescriptor::truncate shrinks or expands regular files; expansion and physical allocation
retain the underlying platform and filesystem semantics.

SC::FileDescriptor::sync requests that file data and metadata reach stable storage, while
SC::FileDescriptor::syncData focuses on file data where the platform distinguishes the two. These calls provide the OS
durability primitive, not a transaction: applications still need their own ordering and recovery protocol when replacing
structured state safely.

# Pipes: descriptors for process communication

SC::PipeDescriptor groups a read endpoint and a write endpoint. SC::PipeDescriptor::createPipe creates an anonymous pipe,
blocking and non-inheritable by default. Per-end inheritance options are intended for child-process redirection; keeping
unneeded pipe ends inheritable can prevent readers from ever observing EOF. The [Process](@ref library_process) library
uses these descriptors to connect standard input, output and error across processes.

Set SC::PipeOptions::blocking to `false` when the endpoints will be registered with [Async](@ref library_async). This flag
changes native descriptor behavior; it does not make the synchronous File calls asynchronous. A synchronous read on a
blocking pipe still waits for data or EOF.

SC::FileDescriptor::openStdInDuplicate, SC::FileDescriptor::openStdOutDuplicate and
SC::FileDescriptor::openStdErrDuplicate return independently owned duplicates. Closing them is safe because the original
process-wide standard handle remains open.

# Named pipes: discoverable local endpoints

Anonymous pipes are exchanged during process creation. Named pipes instead let independently started local processes find
one another through a name. SC::NamedPipeName::build turns a logical token into the native convention: a filesystem path
under `/tmp` (or another absolute directory) on POSIX, and a `\\.\pipe\...` name on Windows. Logical names cannot be empty
or contain path separators.

The server owns the listening endpoint. Each SC::NamedPipeServer::accept and SC::NamedPipeClient::connect produces a
connected SC::PipeDescriptor, so established connections use the same read, write, lifetime and Async integration rules
as other pipes. The server can continue accepting later connections until closed.

This source-backed example shows the complete lifecycle:

\snippet Tests/Libraries/File/FileTest.cpp NamedPipeServerSnippet

On POSIX, a named endpoint occupies a filesystem path. Server options control whether a stale endpoint is removed before
creation and whether the endpoint is removed when the server closes. On Windows, clients may also configure a connection
timeout. These are real platform differences exposed as targeted options rather than hidden behind surprising defaults.

# Boundaries and tradeoffs

File is a low-level portability layer, not a buffered stream framework or serializer. It does not maintain an application
buffer, decode text, frame messages, retry a partial application protocol, or make several operations atomic. A write is
one native write operation: a short write returns failure without exposing the partial byte count, so callers that need
non-blocking retry accounting must use the Async APIs or another suitable layer. This keeps ownership and allocation
under the caller's control, but leaves higher-level policy to the layer that understands the data.

The synchronous API is appropriate for startup work, command-line tools, worker threads and bounded local operations. It
is a poor fit for latency-sensitive event-loop threads when storage or pipe peers can stall. For those cases, open or
create descriptors with non-blocking/Async-compatible options and compose them through [Async](@ref library_async) or
[Async Streams](@ref library_async_streams).

For the complete API surface and option fields, see the [File module](@ref group_file).

# Status
🟩 Usable  
This library now covers synchronous descriptor I/O together with descriptor metadata, synchronization and
descriptor-bound permission/ownership updates.

# Blog

Some relevant blog posts are:

- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)
- [August 2025 Update](https://pagghiu.github.io/site/blog/2025-08-31-SaneCppLibrariesUpdate.html)
- [February 2026 Update](https://pagghiu.github.io/site/blog/2026-02-28-SaneCppLibrariesUpdate.html)
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟦 Complete Features:
- Synchronous files, anonymous pipes and named pipes

💡 Unplanned Features:
- `sendfile`, deferred until a dependency-safe cross-library API is defined

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/File`.
Single File counts
`SaneCppFile.h`.
Standalone counts `SaneCppFileStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 200		| 1364		| 1564	|
| Single File | 1203		| 1815		| 3018	|
| Standalone  | 1203		| 1815		| 3018	|
