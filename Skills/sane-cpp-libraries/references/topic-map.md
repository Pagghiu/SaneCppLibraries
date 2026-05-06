# Topic Map

Use this page when the request is broad or you need to pick the first guide quickly.

| Request shape | Start with | Companion guides |
| --- | --- | --- |
| Adopt Sane in an existing project | [adoption/guide.md](adoption/guide.md) | `build`, `plugin-build`, `examples` |
| Find the best example, test, or doc page | [examples/guide.md](examples/guide.md) | The library-specific guide for the chosen path |
| Adapt STL or exception-heavy code to Sane style | [core-patterns/guide.md](core-patterns/guide.md) | `foundation`, `memory`, `containers`, `strings` |
| Choose base types like `Result`, `Span`, `StringSpan`, `Function` | [foundation/guide.md](foundation/guide.md) | `memory`, `core-patterns` |
| Choose buffers, allocators, or owned strings | [memory/guide.md](memory/guide.md) | `foundation`, `containers`, `strings` |
| Choose a container or understand capacity behavior | [containers/guide.md](containers/guide.md) | `memory`, `containers-reflection` |
| String formatting, parsing, UTF, paths, console text | [strings/guide.md](strings/guide.md) | `memory` |
| Time sources, durations, parsing, scheduling basics | [time/guide.md](time/guide.md) | `async` |
| Threads, locks, semaphores, barriers, thread pool | [threading/guide.md](threading/guide.md) | `async` for I/O-driven coordination |
| Event loops, async requests, wake-up integration | [async/guide.md](async/guide.md) | `file`, `process`, `socket`, `async-streams` |
| C++20 coroutine syntax over Async | [await/guide.md](await/guide.md) | `async`, `socket`, `testing` |
| Async stream pipelines and backpressure | [async-streams/guide.md](async-streams/guide.md) | `async`, `http`, `http-client` |
| Pick the right async networking stack | [async-networking/guide.md](async-networking/guide.md) | `async`, `socket`, `http`, `http-client` |
| Blocking sockets or DNS lookup | [socket/guide.md](socket/guide.md) | `async` if the task should stop blocking |
| HTTP server, parser, body framing, file server | [http/guide.md](http/guide.md) | `async`, `async-streams` |
| Native HTTP client or streaming HTTP bodies | [http-client/guide.md](http-client/guide.md) | `async-streams` |
| File descriptors, byte I/O, pipes, named pipes | [file/guide.md](file/guide.md) | `process`, `async`, `filesystem` |
| Path mutation, copy, delete, rename, links | [filesystem/guide.md](filesystem/guide.md) | `filesystem-iterator`, `file`, `strings` |
| Traverse directories or recurse file trees | [filesystem-iterator/guide.md](filesystem-iterator/guide.md) | `filesystem`, `filesystem-watcher` |
| React to file or folder changes | [filesystem-watcher/guide.md](filesystem-watcher/guide.md) | `async`, `plugin-build` |
| Spawn child processes and wire stdio | [process/guide.md](process/guide.md) | `file`, `filesystem`, `async` |
| Configure and open serial ports | [serial-port/guide.md](serial-port/guide.md) | `file`, `process` |
| Add reflection metadata to types | [reflection/guide.md](reflection/guide.md) | `containers-reflection`, `serialization` |
| Reflect or serialize Sane containers | [containers-reflection/guide.md](containers-reflection/guide.md) | `containers`, `reflection` |
| Choose a serialization approach end to end | [serialization/guide.md](serialization/guide.md) | `serialization-binary`, `serialization-text`, `containers-reflection` |
| Binary persistence or versioned binary loading | [serialization-binary/guide.md](serialization-binary/guide.md) | `reflection`, `containers-reflection` |
| Text or JSON persistence | [serialization-text/guide.md](serialization-text/guide.md) | `reflection`, `containers-reflection` |
| Hash bytes, files, streams, or cache keys | [hashing/guide.md](hashing/guide.md) | `file`, `tools`, `build` |
| Runtime plugins or hot reload | [plugin/guide.md](plugin/guide.md) | `plugin-build`, `async`, `filesystem-watcher` |
| Configure SC::Build or native backends | [build/guide.md](build/guide.md) | `plugin-build`, `tools`, `adoption` |
| Use SC.sh, SC.bat, or custom tools | [tools/guide.md](tools/guide.md) | `build`, `testing` |
| Read, write, or run SCTest | [testing/guide.md](testing/guide.md) | `examples`, the library-specific guide |
