# Library Selection Map

Use this file when the user starts with a task instead of a library name.

## Pick By Goal

### Core language and ownership model

- Foundation: primitive types, `Result`, `Span`, `Function`, compile-time/platform helpers
- Memory: allocators, buffers, segments, virtual memory
- Containers: Sane-owned container types when the user wants Sane containers instead of `std::`
- Strings: formatting, conversion, paths, command-line parsing, console output
- Threading: threads, atomics, mutexes, semaphores, barriers, thread pool
- Time: relative, absolute, and high-resolution time handling

### Files and local system work

- File: synchronous file I/O
- FileSystem: existence checks and file or directory operations
- FileSystemIterator: directory enumeration
- FileSystemWatcher: change notifications for files and directories
- Process: child processes and redirected I/O
- SerialPort: serial device configuration and access

### Reflection and persistence

- Reflection: describe C++ types at compile time
- ContainersReflection: bridge Sane containers into reflection and serialization
- SerializationBinary: compact binary persistence
- SerializationText: text or JSON-oriented persistence

### Networking and async

- Socket: synchronous networking and DNS
- Async: event-loop-driven async I/O across files, sockets, timers, processes, and watch events
- AsyncStreams: readable, writable, and transform stream composition
- Http: HTTP parser plus async server and client stack
- HttpClient: streaming-first HTTP client with native OS backends

### Advanced extensibility and tooling

- Plugin: runtime-compiled plugins and hot reload
- Build: build generation and native backend workflows
- Tools: C++-script-style repository tools and custom tool entrypoints

## Default Recommendations

- New adopter with unknown needs: start with Foundation and the adoption guide.
- User wants examples first: route to `examples`.
- User wants to persist app state: Reflection plus one serialization skill.
- User wants network server work: Async, AsyncStreams, and Http.
- User wants the smallest possible HTTP client path: try HttpClient first.
- User wants hot reload: Plugin plus FileSystemWatcher, with SCExample as the main example.

## Dependency Reminders

- Foundation underpins almost every library.
- Memory and Containers are optional for many adopters.
- Http composes with Async and AsyncStreams.
- Plugin depends on Process and Strings, and effectively pulls in File and Foundation transitively.
- SerializationText depends on Reflection and Strings.
