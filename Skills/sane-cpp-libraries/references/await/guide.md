# Sane Await

## Quick Use

Choose `await` when the task is specifically about the experimental C++20 coroutine layer over `AsyncEventLoop`.

- Treat `Await` as MVP / Experimental: inspect `Documentation/Libraries/Await.md`, `Libraries/Await/Await.h`, and the
  tests before making API claims.
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
  the currently suspended operation. `AwaitTaskRegistry` covers explicit fixed-slot detached/background task ownership.
- Prefer fixed `AwaitAllocator` examples when discussing no-hidden-allocation coroutine frame storage.
- Use `AwaitAllocator::capacity()`, `used()`, `peakUsed()`, `largestAllocationSize()`, `failedAllocationSize()`, and
  `statistics()` when sizing caller-provided coroutine frame storage.
- Mention `createVirtual()`, `createMalloc()`, and `createPolymorphic()` only as explicit opt-in allocation modes.
  `createVirtual()` is for deliberate reservation/commit experiments, not the normal no-hidden-allocation story.
- Use `Examples/AwaitEcho` for socket connect/accept/receive/sendAll/task groups.
- Use `Examples/AwaitBackgroundDigest` for ThreadPool-backed CPU work through `loopWork()`.
- Use `Examples/AwaitBackgroundJobs` for detached/background jobs through fixed caller-owned `AwaitTaskRegistry` slots.
- Use `Examples/AwaitFirstResponse` for registry `waitAny()` races that cancel slower background responses.
- Use `Examples/AwaitConfigReload` for the one-child `spawnAndWait()` convenience pattern.
- Use `Examples/AwaitDeadline` for child task deadlines through `waitFor()` and `AwaitTimeoutResult`.
- Use `Examples/AwaitDatagramPing` for UDP sendTo/receiveFrom request/reply flows.
- Use `Examples/AwaitFileCourier` for file copy plus `fileSend()` workflows.
- Use `Examples/AwaitFilePatch` for offset `fileWrite()` followed by `fileRead()` with caller-owned buffers.
- Use `Examples/AwaitLineProtocol` for CRLF text protocols using `receiveLine()` plus `sendAll()`.
- Use `Examples/AwaitManifestPreview` for bounded `fileReadUntilFullOrEOF()` into caller-owned preview storage.
- Use `Examples/AwaitProcessExitCodes` for child process exit waits with `processExit()`.
- Use `Examples/AwaitServiceProbe` for a compact application-shaped flow combining sockets, nested task groups, timeout
  cancellation, and fixed allocator diagnostics.
- Use `Examples/AwaitThreadWakeUp` for external thread notifications through `AwaitLoopWakeUp`.
- Use single-buffer `sendAll()` for contiguous payloads and scatter/gather `sendAll()` with caller-owned
  `Span<const char>` storage when header/body fragments should be sent as one logical stream message.
- Use `fileRead()` / `fileWrite()` with `SerialDescriptor`; do not add dedicated serial awaiter names unless `Async`
  grows serial-specific request types.

## What To Watch

- `SCAwaitTest` is C++20 and standard-library enabled; it is intentionally separate from `SCTest`.
- `SCAwaitTest` owns allocator coverage; Await stays separate from `SCTest` because it needs C++20 and standard-library
  coroutine support.
- `AwaitTask` is caller-owned, movable, non-copyable, and must not be destroyed while active.
- Use task state helpers to keep shutdown code explicit: `AwaitTask::isActive()`, `isCompleted()`,
  `AwaitTaskRegistry::hasActiveTasks()`, `hasCompletedTasks()`, and the group/registry `remainingCapacity()` helpers
  are query-only and never allocate, cancel, or drain.
- Completed child task frames can be destroyed while an `Async` callback is unwinding; `AwaitEventLoop` defers that
  destruction until `run()`, `runOnce()`, or `runNoWait()` returns so embedded `AsyncRequest` storage stays alive.
- Result spans such as `AwaitSocketReceiveResult::data` and `AwaitFileReadResult::data` point into caller-provided
  buffers; those buffers must outlive result inspection.
- Child tasks can still be explicitly `spawn()`-ed before `co_await child`; use `spawnAndWait()` when a parent should
  start and await a child in one expression.
- Keep `spawnAndWait()` for the one-child convenience case; its name intentionally exposes "starts then awaits" behavior.
  Use `AwaitTaskGroup` for multiple children, aggregation, `waitAny()`, or custom child cancellation policy.
- Do not add destructor-based cancel/drain helpers unless the design has an explicit way to surface failures. Prefer
  explicit `Result`-returning shutdown (`cancelAll()`, run the loop, then `clearCompleted()`).
- Destroying an active `AwaitTask` is still an assert-release programming error. Use `isActive()` and
  `AwaitTaskRegistry::hasActiveTasks()` when shutdown code needs a pre-destroy diagnostic.
- Use `AwaitTaskGroup` with caller-provided `Span<AwaitTask*>` storage when a parent needs to own several children and
  cancel/wait them as a group. `waitAny()` defaults to cancelling remaining children before returning so stack-owned
  child tasks are not left active.
- Use `AwaitTaskGroup::spawnAll()` when children are already listed in a caller-owned `AwaitTask*` array; it reduces
  repeated `spawn()` calls without hiding task storage.
- Use `AwaitTaskGroup::summarizeResults()` when only aggregate counts and first-failure metadata are needed; use
  `collectResults()` when each child `Result` must be copied into caller-provided storage.
- Cancellation is cooperative and routed through the currently suspended awaiter.
- Use `AwaitIsCancelled(result)` to distinguish cooperative cancellation from ordinary failure without introducing
  `Result<T>` or exception-style control flow.
- Treat cancellation as best-effort and idempotent after request: active `AsyncRequest` gets stopped when possible,
  already-completed tasks keep normal result.
- `AwaitLoopWakeUp` is the stable object shared with another thread or callback; the coroutine waits with
  `co_await await.wakeUp(wakeUp, result)` and the producer calls `wakeUp.wakeUp(await)`.
- Use `receive()` for one-shot "some bytes" socket reads and `receiveExact()` when the caller buffer must be filled or
  fail on early disconnect.
- Use `receiveLine()` for simple CRLF/LF text protocols when the caller can provide a maximum line buffer.
- Use `fileReadUntilFullOrEOF()` when file-reading code should fill caller storage unless EOF arrives first.
- Do not invent `fileWriteAll()` at the Await layer unless `AsyncFileWrite` semantics change; current file writes already
  complete the provided single or scatter/gather buffer, or return an error.
- Prefer single-buffer `fileWrite()` when using explicit offsets in portable examples; treat scatter/gather file writes
  with offsets as backend-sensitive until `AsyncFileWrite` documents stronger semantics.
- Keep thin no-allocation convenience helpers on `AwaitEventLoop`; move protocol adapters or helpers with extra stable
  state into explicit `Await*` structs.
- Do not add direct filesystem watcher awaiters on `AwaitEventLoop` yet. `FileSystemWatcher` is a long-lived callback
  stream; use `FileSystemWatcherAsyncT<AsyncEventLoop>` on the same loop, or design a caller-owned `Await*` adapter when
  a concrete stream/channel workflow is needed.
- If designing such an adapter, keep it bounded and caller-owned: watcher/folder state, event queue storage, wake-up
  object, overflow reporting, and backpressure policy must all be explicit.
- `waitFor()` cancels the child task when the timeout expires and reports timeout state through `AwaitTimeoutResult`.
- `fsRead()` and `fsWrite()` wrap `AsyncFileSystemOperation::read()` and `write()`; those operations borrow file handles
  and preserve caller ownership.
- Operation results follow Sane conventions: awaiters return plain `Result`, and extra data is written into explicit
  caller-provided result objects such as `AwaitSocketReceiveResult` or `AwaitFileReadResult`.
- New awaiters must keep returning plain `Result`; if more information is needed, add an explicit result object parameter.
- Keep `spawn()` immediate for now. Revisit only if Await grows a ready queue or runnable tasks that are not backed by an
  active `AsyncRequest`.
- Prefer structured children through `AwaitTaskGroup`. If detached/background tasks are needed, use
  `AwaitTaskRegistry` with caller-owned `Span<AwaitTask>` storage, explicit shutdown cancellation, and no hidden
  allocation. `AwaitTaskRegistry::waitAll()` drains currently registered tasks and `waitAny()` waits for the first
  completed slot without building a separate task-pointer array.
- Treat `CancelRemaining` as the structured `waitAny()` default and `LeaveRemainingRunning` as the explicit escape
  hatch. `AwaitTaskGroupCancelPolicy::CancelChildren` is about parent cancellation while waiting in a group.
- It is fair to describe `AwaitTaskGroup` as asyncio-like control flow, but always call out the Sane C++ difference:
  task objects, pointer arrays, result arrays, buffers, and allocator storage remain caller-owned and explicit.
- For registry shutdown, use an explicit cancel/drain/clear sequence: `registry.cancelAll()`, run the owning
  `AwaitEventLoop`, then `registry.clearCompleted(&summary)`. Repeated `cancelAll()` and `clearCompleted()` calls are
  valid and covered by tests.
- For fixed allocator sizing, inspect `AwaitAllocator::peakUsed()`, `largestAllocationSize()`, or
  `statistics().peakBytesInUse` after realistic runs and keep enough headroom for concurrently active coroutine frames.
  Completed tasks may keep frames allocated until their `AwaitTask` object is destroyed or cleared from a registry.
- Keep example allocator storage modest and intentional; socket examples should expose peak/largest/capacity diagnostics
  instead of hiding behind an oversized fixed buffer.
- The strict no-stdlib coroutine story is not solved yet; do not present `Await` as ready for `SC_DISABLE_STD_CPP=1` or
  normal `-nostdinc++` use. A shim would need coroutine traits/handles/suspend types plus compiler builtin mapping. This
  is stable-track work, not required for MVP usage.
- The C++20 Await targets are built with exceptions disabled by default; keep using `Result` and `SC_CO_TRY`.
- `AwaitEventLoop::filePoll()` fails fast on Windows instead of hanging because `AsyncFileReadiness` is currently only
  useful on the POSIX backends for normal file/pipe handles.
- For validation, run the smallest relevant `SCAwaitTest` section on macOS, then Linux, then Windows.

## References

- Public docs: `Documentation/Libraries/Await.md`
- Public header: `Libraries/Await/Await.h`
- Tests: `Tests/Libraries/Await/AwaitTest.cpp`
- Test target: `Tests/SCAwaitTest/SCAwaitTest.cpp`
