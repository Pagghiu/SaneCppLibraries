---
name: sane-file-system-iterator
description: Directory traversal and recursive enumeration. Use when an AI agent needs to walk file trees, filter entries, or inspect directory contents with Sane C++.
---

# Sane File System Iterator

## Overview

Use this skill for enumerating files and directories. Keep it for traversal and recursion; use `sane-file-system` to mutate paths and `sane-file` to open file contents.

## Start Here

- Inspect `Documentation/Libraries/FileSystemIterator.md`.
- Read `Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp`.
- Check `Libraries/FileSystemIterator/FileSystemIterator.h`.

## Use It For

- Iterate a directory once or recursively.
- Build file selection logic before running copy, delete, watch, or process workflows.
- Route path iteration output into higher-level tools or custom filters.

## Platform Notes

- On Windows, expect UTF8 input or UTF16 input and UTF16 output.
- On POSIX, expect and return UTF8 paths.
- Treat the iterator as a discovery tool, not a mutation tool.

## Prefer These Companions

- Use `sane-file-system` after discovery if you need to copy or remove entries.
- Use `sane-file-system-watcher` if the goal is reacting to changes over time.
- Use `sane-strings` when path normalization or formatting matters.

## Pitfalls

- Do not use it as a filesystem mutation API.
- Do not assume it exposes file contents.
- Do not duplicate watcher behavior in a traversal skill.

## References

- [Traversal reference](references/iteration-patterns.md)
