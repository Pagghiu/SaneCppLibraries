---
name: sane-file-system
description: Filesystem operations for files and directories. Use when an AI agent needs to create, copy, delete, rename, inspect, or link paths with Sane C++ instead of reading or writing file bytes.
---

# Sane File System

## Overview

Use this skill for path-level filesystem work. Keep it for directory creation, existence checks, copy and delete flows, links, permissions, and file metadata. Use `sane-file` for actual byte I/O.

## Start Here

- Inspect `Documentation/Libraries/FileSystem.md`.
- Read `Tests/Libraries/FileSystem/FileSystemTest.cpp` and `Tests/Libraries/FileSystem/PathTest.cpp`.
- Check `Libraries/FileSystem/FileSystem.h` and `Libraries/Strings/Path.h`.

## Use It For

- Initialize a base directory and resolve later relative paths against it.
- Create, copy, remove, rename, or query files and directories.
- Create hard links or symbolic links and read link targets.
- Check access, permissions, ownership, and timestamps.

## Prefer These Companions

- Use `sane-file` for descriptor I/O and pipe handling.
- Use `sane-file-system-iterator` to enumerate directory contents.
- Use `sane-strings` for path parsing and composition when needed.

## Pitfalls

- Do not use this for reading or writing bytes.
- Do not assume paths are interpreted relative to the current working directory.
- Do not duplicate `sane-file-system-iterator` responsibilities.

## References

- [Filesystem operations reference](references/fs-operations.md)
