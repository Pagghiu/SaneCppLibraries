# Sane File System Iterator

## Overview

Use this guide for enumerating files and directories. Keep it for traversal and recursion; use `filesystem` to mutate paths and `file` to open file contents.

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

- Use `filesystem` after discovery if you need to copy or remove entries.
- Use `filesystem-watcher` if the goal is reacting to changes over time.
- Use `strings` when path normalization or formatting matters.

## Pitfalls

- Do not use it as a filesystem mutation API.
- Do not assume it exposes file contents.
- Do not duplicate watcher behavior in a traversal skill.

## References

- [Traversal reference](references/iteration-patterns.md)
