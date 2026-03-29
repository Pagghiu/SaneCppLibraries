---
name: sane-file
description: Synchronous file descriptor, pipe, and named pipe I/O. Use when an AI agent needs to read or write bytes, inspect descriptor metadata, set blocking or inheritable state, or build IPC and process-redirection flows in Sane C++ integrations.
---

# Sane File

## Overview

Use this skill for descriptor-based file and pipe I/O. Keep it for byte streams, metadata, and IPC endpoints. Hand off path manipulation, directory changes, copy or delete workflows to `sane-file-system`.

## Start Here

- Inspect `Documentation/Libraries/File.md`.
- Read `Tests/Libraries/File/FileTest.cpp` for working snippets.
- Check `Libraries/File/File.h` for the public API surface.

## Use It For

- Open, read, write, seek, sync, truncate, or query a file descriptor.
- Create or consume unnamed pipes for child-process wiring.
- Create named pipe servers or clients for cross-process communication.
- Mark descriptors inheritable before passing them into `sane-process`.

## Prefer These Companions

- Use `sane-file-system` for filesystem paths, directories, and mutations.
- Use `sane-process` when the file or pipe is part of a spawned child process.
- Use `sane-async` or `sane-async-streams` only when the workflow is explicitly asynchronous.

## Pitfalls

- Do not treat this as a filesystem library.
- Do not assume hidden allocations or STL types.
- Keep buffers caller-owned and size them explicitly.

## References

- [File I/O reference](references/file-io-patterns.md)
