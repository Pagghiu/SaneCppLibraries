# Sane Async

## Quick Use

Choose `async` when the task needs the Sane event loop, request-driven completion callbacks, or external loop integration.

- Use `AsyncEventLoop` for `run`, `runOnce`, `runNoWait`, or explicit submit/poll/dispatch control.
- Use `AsyncLoopTimeout` for deadlines that must wake a blocked loop; a wall-clock check around `runOnce()` cannot wake a quiet poll.
- Use `AsyncEventLoopMonitor` when another loop or thread must wake the async loop.
- Keep every `AsyncRequest`-derived object in stable memory until the callback finishes.
- Use `AsyncSequence` when request ordering matters.
- Use `await` as the companion guide when discussing the Draft C++20 coroutine wrapper over `AsyncEventLoop`.

## What To Watch

- Pair `async` with `socket` for socket I/O.
- Pair it with `file` and `process` for file and process events.
- Pair it with `async-streams` when request data should flow through stream pipelines.
- `AsyncFileSystemOperation::read()` and `write()` borrow raw file handles and preserve caller ownership; only
  `close()` consumes the handle.
- `AsyncFileRead::executeOn(sequence, threadPool)` is the documented escape hatch for blocking descriptors. It moves the
  blocking operation to a worker; it does not make that descriptor nonblocking and does not satisfy a no-blocking-read
  contract.
- `AwaitEventLoop` wraps an existing `AsyncEventLoop&`; it should not be described as a replacement for callback-style
  `Async`, because both styles can coexist on the same loop.

## References

- [event loop and request lifecycle](references/event-loop-and-request-lifecycle.md)
- Public docs: `Documentation/Libraries/Async.md`
- Tests: `Tests/Libraries/Async/AsyncTest.cpp`
- Examples: `Examples/SCExample/HotReloadSystem.h`
