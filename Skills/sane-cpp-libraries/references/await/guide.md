# Sane Await

## Quick Use

Choose `await` when the task is specifically about the draft C++20 coroutine layer over `AsyncEventLoop`.

- Treat `Await` as Draft: inspect `Documentation/Libraries/Await.md`, `Libraries/Await/Await.h`, and the tests before
  making API claims.
- Use `AwaitEventLoop` as a wrapper around an existing `AsyncEventLoop&`; it does not own or close the async loop.
- Keep Sane style: coroutine functions return `AwaitTask`, completion returns plain `Result`, and extra outputs use caller-provided objects.
- Use `SC_CO_TRY(co_await ...)` inside coroutine bodies instead of `SC_TRY`.
- Keep callback-style `Async` compatibility: `Await` operations share the same underlying event loop.
- Current awaiter coverage includes sleep, loop wake-up, socket accept/connect/send/scatter-gather send/sendAll/receive,
  datagram sendTo/scatter-gather sendTo/receiveFrom, fileRead/fileWrite/scatter-gather fileWrite/fileSend/filePoll, fsOpen/fsClose/fsRead/fsWrite/fsCopyFile/
  fsCopyDirectory/fsRename/fsRemoveEmptyDirectory/fsRemoveFile, processExit, one-shot signal, loopWork, child tasks,
  `spawnAndWait()`, `AwaitTaskGroup::waitAll()`, `AwaitTaskGroup::waitAny()`, child task timeouts with `waitFor()`,
  and cancellation of the currently suspended operation.
- Prefer `AwaitArena` examples when discussing no-allocation coroutine frame storage. The draft still allows no-arena
  standard nothrow allocation for ergonomic experiments; production-style examples should pass an arena.
- Use `Examples/AwaitEcho` as the readable showcase example for socket connect/accept/receive/sendAll/task groups.

## What To Watch

- `SCAwaitTest` is C++20 and standard-library enabled; it is intentionally separate from `SCTest`.
- `AwaitTask` is caller-owned, movable, non-copyable, and must not be destroyed while active.
- Result spans such as `AwaitSocketReceiveResult::data` and `AwaitFileReadResult::data` point into caller-provided
  buffers; those buffers must outlive result inspection.
- Child tasks can still be explicitly `spawn()`-ed before `co_await child`; use `spawnAndWait()` when a parent should
  start and await a child in one expression.
- Use `AwaitTaskGroup` with caller-provided `Span<AwaitTask*>` storage when a parent needs to own several children and
  cancel/wait them as a group. `waitAny()` defaults to cancelling remaining children before returning so stack-owned
  child tasks are not left active.
- Cancellation is cooperative and routed through the currently suspended awaiter.
- `AwaitLoopWakeUp` is the stable object shared with another thread or callback; the coroutine waits with
  `co_await await.wakeUp(wakeUp, result)` and the producer calls `wakeUp.wakeUp(await)`.
- `waitFor()` cancels the child task when the timeout expires and reports timeout state through `AwaitTimeoutResult`.
- `fsRead()` and `fsWrite()` wrap `AsyncFileSystemOperation::read()` and `write()`; those operations borrow file handles
  and preserve caller ownership.
- Operation results follow Sane conventions: awaiters return plain `Result`, and extra data is written into explicit
  caller-provided result objects such as `AwaitSocketReceiveResult` or `AwaitFileReadResult`.
- The no-stdlib coroutine story is not solved yet; do not present `Await` as ready for normal `-nostdinc++` use.

## References

- Public docs: `Documentation/Libraries/Await.md`
- Public header: `Libraries/Await/Await.h`
- Tests: `Tests/Libraries/Await/AwaitTest.cpp`
- Test target: `Tests/SCAwaitTest/SCAwaitTest.cpp`
