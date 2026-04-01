# Process Launch And Redirection

## Start Here

- `Documentation/Libraries/Process.md`
- `Libraries/Process/Process.h`
- `Libraries/File/File.h`
- `Tests/Libraries/Process/ProcessTest.cpp`

## Use `process` For

- Launching a process and waiting for exit.
- Building a process chain.
- Redirecting stdin, stdout, or stderr.
- Setting the working directory or environment variables before launch.

## Key Types

- `SC::Process`
- `SC::ProcessChain`
- `SC::ProcessEnvironment`
- `SC::ProcessFork`

## Common Patterns

- Use isolated process execution when you only need a single child.
- Use process chaining when stdout from one child feeds another.
- Use file descriptors or pipes from `file` for redirection endpoints.

## Hand Off To Other Skills

- Use `file` for pipe descriptors and inheritance.
- Use `async` when child I/O is part of an event loop.
- Use `filesystem` for preparing the working directory or input paths.

## Pitfalls

- Do not merge process launch guidance with file-content guidance.
- Do not treat `Process` and `ProcessChain` as interchangeable.
- Do not hide environment or cwd setup when the caller may need reproducible launches.
