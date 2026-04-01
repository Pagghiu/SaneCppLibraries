# Sane Async

## Quick Use

Choose `async` when the task needs the Sane event loop, request-driven completion callbacks, or external loop integration.

- Use `AsyncEventLoop` for `run`, `runOnce`, `runNoWait`, or explicit submit/poll/dispatch control.
- Use `AsyncEventLoopMonitor` when another loop or thread must wake the async loop.
- Keep every `AsyncRequest`-derived object in stable memory until the callback finishes.
- Use `AsyncSequence` when request ordering matters.

## What To Watch

- Pair `async` with `socket` for socket I/O.
- Pair it with `file` and `process` for file and process events.
- Pair it with `async-streams` when request data should flow through stream pipelines.

## References

- [event loop and request lifecycle](references/event-loop-and-request-lifecycle.md)
- Public docs: `Documentation/Libraries/Async.md`
- Tests: `Tests/Libraries/Async/AsyncTest.cpp`
- Examples: `Examples/SCExample/HotReloadSystem.h`
