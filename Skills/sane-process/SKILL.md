---
name: sane-process
description: Child process launch, chaining, redirection, and environment control. Use when an AI agent needs to spawn processes, connect stdio streams, or configure working directories and environment variables with Sane C++.
---

# Sane Process

## Overview

Use this skill for launching and chaining child processes. Keep it centered on process creation, stdio wiring, working directory selection, and environment setup.

## Start Here

- Inspect `Documentation/Libraries/Process.md`.
- Read `Tests/Libraries/Process/ProcessTest.cpp`.
- Check `Libraries/Process/Process.h` and `Libraries/File/File.h`.

## Use It For

- Launch a process in isolation and wait for completion.
- Build a process chain with connected stdin and stdout streams.
- Set a working directory or environment before launch.
- Redirect stdio through file or pipe descriptors.

## Prefer These Companions

- Use `sane-file` for pipe endpoints and descriptor control.
- Use `sane-async` when process I/O must integrate with an event loop.
- Use `sane-file-system` when the process setup depends on paths or directories.

## Pitfalls

- Do not use this for generic file I/O.
- Do not forget that process chaining and isolated process execution are distinct workflows.
- Do not hide the redirection model behind STL-style abstractions.

## References

- [Process reference](references/process-launch-and-redirection.md)
