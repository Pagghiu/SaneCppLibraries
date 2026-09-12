# Source anchors for non-obvious API composition

These paths are checked against this checkout. Re-check them when upgrading Sane.

| Claim | Source to inspect |
| --- | --- |
| Pipe options apply to pipe creation | `Libraries/File/File.h` (`PipeOptions`, `PipeDescriptor::createPipe`); `Documentation/Libraries/File.md` pipes section |
| Process launch accepts redirection wrappers/descriptors | `Libraries/Process/Process.h` (`Process::launch`); `Tests/Libraries/Async/AsyncTestProcess.inl` |
| `AsyncFileRead` needs caller buffer and signals EOF | `Libraries/Async/Async.h` (`AsyncFileRead`); `Tests/Libraries/Async/AsyncTestProcess.inl` |
| Process exit is a separate request | `Libraries/Async/Async.h` (`AsyncProcessExit`); `Tests/Libraries/Async/AsyncTestProcess.inl` |
| Active requests are caller-owned/stable | `Architecture/Async/async-0001-keep-asyncrequest-objects-caller-owned-and-memory-stable.md` |
| `runOnce` blocks and loop timeouts wake it | `Documentation/Libraries/Async.md` (driving the loop, timers); `Libraries/Async/Async.h` (`AsyncEventLoop::runOnce`, `AsyncLoopTimeout`) |
| Blocking file operations can execute on a worker sequence | `Libraries/Async/Async.h` (`AsyncFileRead`, `AsyncRequest::executeOn`); `Tests/Libraries/Async/AsyncTestProcess.inl` |
| Process launch retains caller-visible state and reaping is platform-sensitive | `Libraries/Process/Process.h`; `Libraries/Async/Async.h` (`AsyncProcessExit`); selected `Libraries/Async/Internal/Async*` backend |
| Recursive traversal capacity is caller chosen | `Architecture/FileSystemIterator/filesystemiterator-0001-make-recursive-enumeration-caller-bounded.md` |

Do not treat these anchors as a substitute for the selected API's header: overloads and platform details are authoritative there.
