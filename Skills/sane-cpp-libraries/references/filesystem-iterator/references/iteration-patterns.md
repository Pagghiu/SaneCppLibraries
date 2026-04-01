# Traversal Patterns

## Start Here

- `Documentation/Libraries/FileSystemIterator.md`
- `Libraries/FileSystemIterator/FileSystemIterator.h`
- `Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp`

## Use `filesystem-iterator` For

- Walking a directory once or recursively.
- Selecting paths before a later mutation or watch step.
- Producing an ordered list or filtered set of entries.

## Platform Notes

- On Windows, accept UTF8 or UTF16 input and return UTF16 paths.
- On POSIX, accept and return UTF8 paths.

## Common Patterns

- Use recursive traversal when a whole tree matters.
- Use non-recursive traversal when only the top level matters.
- Pass discovered paths into `filesystem`, `file`, or `filesystem-watcher` depending on the next step.

## Hand Off To Other Skills

- Use `filesystem` for create, copy, delete, or rename flows.
- Use `filesystem-watcher` for continuous change notification.
- Use `process` if discovery is used to build a command invocation.

## Pitfalls

- Do not present traversal as mutation.
- Do not use it for file contents.
- Do not hide platform encoding differences from the user-facing explanation.
