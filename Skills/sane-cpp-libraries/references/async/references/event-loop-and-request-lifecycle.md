# Event Loop And Request Lifecycle

## Use This When

- Drive the event loop from a third-party application.
- Start, stop, or wake async requests.
- Integrate Sane async I/O into a GUI loop or another scheduler.

## Core Model

- `SC::AsyncEventLoop` owns the dispatch cycle.
- `SC::AsyncRequest`-derived objects must stay alive and memory-stable until completion.
- `SC::AsyncEventLoopMonitor` bridges external threads or external loops.
- `SC::AsyncSequence` helps when request ordering matters.

## Run Modes

- `run` for a self-contained loop.
- `runOnce` for a single iteration.
- `runNoWait` for polling without blocking.
- `submitRequests`, `blockingPoll`, and `dispatchCompletions` for explicit control.

`runOnce` can wait indefinitely when active requests are quiet. If a deadline must bound that wait, start an
`AsyncLoopTimeout` on the same loop before entering `runOnce`, or arrange a loop wake-up from the deadline owner. Merely
checking a monotonic clock between calls does not provide a bound.

## Request Families

- Socket connect, accept, send, receive, send-to, receive-from.
- File read and write.
- File-system operations.
- Process exit monitoring.
- Loop timeout, wake-up, work, and signal handling.

## Pitfalls

- Keep request objects in stable storage until callback completion.
- Close or stop resources explicitly before destruction.
- Use the matching request family for the descriptor type you already own.
- Check the `Result` returned by every run mode; a poll/dispatch failure is not loop completion.

## Best Sources

- `Documentation/Libraries/Async.md`
- `Tests/Libraries/Async/AsyncTest.cpp`
- `Tests/Libraries/Async/AsyncTestSocketTCP.inl`
- `Tests/Libraries/Async/AsyncTestLoop.inl`
