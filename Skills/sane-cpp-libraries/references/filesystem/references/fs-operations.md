# Filesystem Operations

## Start Here

- `Documentation/Libraries/FileSystem.md`
- `Libraries/FileSystem/FileSystem.h`
- `Libraries/Strings/Path.h`
- `Tests/Libraries/FileSystem/FileSystemTest.cpp`
- `Tests/Libraries/FileSystem/PathTest.cpp`

## Use `filesystem` For

- Resolving paths against a base directory.
- Creating, copying, renaming, or removing files and directories.
- Managing symlinks, hard links, permissions, and timestamps.
- Querying accessibility or metadata for paths.

## Important Behavior

- Call `init` with an absolute base directory when you want relative paths resolved consistently.
- Relative paths are resolved against the initialized base directory, not the current working directory.
- Use `SC::Path` for path parsing and composition.

## Common Patterns

- Check existence before mutating when the call site needs an explicit branch.
- Use `makeDirectoryRecursive` for bootstrapping trees.
- Use `removeDirectoryRecursive` for cleanup flows that own the tree.

## Hand Off To Other Skills

- Use `file` for reading or writing descriptor bytes.
- Use `filesystem-iterator` to discover entries before mutating them.
- Use `strings` for display or normalization logic around paths.

## Pitfalls

- Do not explain it as a file-content API.
- Do not assume path resolution follows the process current directory.
- Do not duplicate traversal logic that belongs in the iterator skill.
