# File I/O Patterns

## Start Here

- `Documentation/Libraries/File.md`
- `Libraries/File/File.h`
- `Tests/Libraries/File/FileTest.cpp`

## Use `file` For

- Opening, reading, writing, seeking, syncing, truncating, and querying a descriptor.
- Working with unnamed pipes and named pipes.
- Passing inherited descriptors into child processes.

## Key Types

- `SC::FileDescriptor`
- `SC::PipeDescriptor`
- `SC::NamedPipeServer`
- `SC::NamedPipeClient`
- `SC::NamedPipeName`

## Common Patterns

- Use caller-owned buffers for read and write calls.
- Use `openStdInDuplicate`, `openStdOutDuplicate`, or `openStdErrDuplicate` when connecting to current-process standard descriptors.
- Use named pipes for IPC when the caller needs a platform-neutral endpoint name.

## Hand Off To Other Skills

- Use `filesystem` for path and directory mutation.
- Use `process` when the descriptor is part of a launched child process.
- Use `async` or `async-streams` for event-loop-driven descriptor handling.

## Pitfalls

- Treat this as synchronous descriptor I/O, not a filesystem helper.
- Keep descriptor ownership explicit.
- Avoid mixing byte I/O guidance with path-level guidance.
