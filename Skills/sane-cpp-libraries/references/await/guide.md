# Sane Await

## Quick Use

Choose `await` when the task is specifically about the draft C++20 coroutine layer over `AsyncEventLoop`.

- Treat `Await` as Draft: inspect `Documentation/Libraries/Await.md`, `Libraries/Await/Await.h`, and the tests before
  making API claims.
- Use `AwaitEventLoop` as a wrapper around an existing `AsyncEventLoop&`; it does not own or close the async loop.
- Keep Sane style: coroutine functions return `AwaitTask`, completion returns plain `Result`, and extra outputs use caller-provided objects.
- Use `SC_CO_TRY(co_await ...)` inside coroutine bodies instead of `SC_TRY`.
- Keep callback-style `Async` compatibility: `Await` operations share the same underlying event loop.
- Current awaiter coverage includes sleep, loop wake-up, socket accept/connect/send/scatter-gather send/sendAll/receive/
  receiveExact/receiveLine, datagram sendTo/scatter-gather sendTo/receiveFrom, fileRead/offset fileRead/
  fileReadUntilFullOrEOF/fileWrite/offset fileWrite/scatter-gather fileWrite/fileSend/POSIX filePoll,
  fsOpen/fsClose/fsRead/fsWrite/fsCopyFile/fsCopyDirectory/fsRename/
  fsRemoveEmptyDirectory/fsRemoveFile, processExit, one-shot signal, loopWork, child tasks, `spawnAndWait()`,
  `AwaitTaskGroup::waitAll()`, `AwaitTaskGroup::waitAny()`, child task timeouts with `waitFor()`, and cancellation of
  the currently suspended operation.
- Prefer `AwaitArena` examples when discussing no-allocation coroutine frame storage. The draft still allows no-arena
  standard nothrow allocation for ergonomic experiments; production-style examples should pass an arena.
- Define `SC_AWAIT_REQUIRE_ARENA=1` when exploring production-style builds that must reject standard coroutine
  allocation fallback.
- Use `Examples/AwaitEcho` for socket connect/accept/receive/sendAll/task groups.
- Use `Examples/AwaitDatagramPing` for UDP sendTo/receiveFrom request/reply flows.
- Use single-buffer `sendAll()` for contiguous payloads and scatter/gather `sendAll()` with caller-owned
  `Span<const char>` storage when header/body fragments should be sent as one logical stream message.

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
- Use `receive()` for one-shot "some bytes" socket reads and `receiveExact()` when the caller buffer must be filled or
  fail on early disconnect.
- Use `receiveLine()` for simple CRLF/LF text protocols when the caller can provide a maximum line buffer.
- Use `fileReadUntilFullOrEOF()` when file-reading code should fill caller storage unless EOF arrives first.
- Do not invent `fileWriteAll()` at the Await layer unless `AsyncFileWrite` semantics change; current file writes already
  complete the provided single or scatter/gather buffer, or return an error.
- Keep thin no-allocation convenience helpers on `AwaitEventLoop`; move protocol adapters or helpers with extra stable
  state into explicit `Await*` structs.
- `waitFor()` cancels the child task when the timeout expires and reports timeout state through `AwaitTimeoutResult`.
- `fsRead()` and `fsWrite()` wrap `AsyncFileSystemOperation::read()` and `write()`; those operations borrow file handles
  and preserve caller ownership.
- Operation results follow Sane conventions: awaiters return plain `Result`, and extra data is written into explicit
  caller-provided result objects such as `AwaitSocketReceiveResult` or `AwaitFileReadResult`.
- The no-stdlib coroutine story is not solved yet; do not present `Await` as ready for normal `-nostdinc++` use.
- The C++20 Await targets are built with exceptions disabled by default; keep using `Result` and `SC_CO_TRY`.
- `AwaitEventLoop::filePoll()` fails fast on Windows instead of hanging because `AsyncFilePoll` is currently only
  useful on the POSIX backends for normal file/pipe handles.

## References

- Public docs: `Documentation/Libraries/Await.md`
- Public header: `Libraries/Await/Await.h`
- Tests: `Tests/Libraries/Await/AwaitTest.cpp`
- Test target: `Tests/SCAwaitTest/SCAwaitTest.cpp`
