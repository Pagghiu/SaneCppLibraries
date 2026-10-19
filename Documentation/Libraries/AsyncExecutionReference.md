@page library_async_execution_reference AsyncExecution reference

@brief Operation contracts, lifetime rules and build settings

[TOC]

Start with the [tutorial](@ref library_async_execution) for examples. This page covers the details needed when selecting
operations, handling cancellation or integrating with an existing loop.

# Completion and consumption

| Completion | Meaning | SC consumer behavior |
| --- | --- | --- |
| `set_value(values...)` | Successful operation | `runInto` assigns values to its outputs; `run` discards them |
| `set_error(Result)` | Operation failure | Returns the original category and error value |
| `set_stopped()` | Cooperative cancellation | Returns `AsyncExecutionError::Stopped` |

`runInto(sender, outputs...)` supports multiple and move-only values; output assignments must be nonthrowing.
Outputs are assigned only on success. An explicitly supplied progress object can change before completion;
keep it separate from root outputs when their preservation matters.

`runIntoWithStop(sender, token, outputs...)` and `run(sender, token)` supply a stop token. Convert foreign error
types into a failing `SC::Result` upstream, for example with `let_error`. A `Result` whose identity is Stopped
still belongs to the error channel when delivered through `set_error`.

Call consumers on the loop owner outside callbacks. Reentrant/concurrent consumption is rejected. They keep
connected storage alive through terminal completion, including unrelated loop interruptions.

# Scheduling and transformations

| Choice | Body/continuation | Successful completion |
| --- | --- | --- |
| `schedule(context.getScheduler())` | Loop owner | Loop owner |
| `schedule(app.getPoolScheduler())` | Worker | Worker |
| `on(poolScheduler, sender)` | Sender starts on pool | Restores caller's scheduler |
| `continues_on(sender, scheduler)` | Downstream continues on selected scheduler | Selected scheduler |
| `app.onPool(procedure)` | Owned worker pool | This runtime's owner loop |
| `context.work(pool, procedure)` | Borrowed worker pool | Context's owner loop |

`starts_on` selects where a sender starts. SC I/O still submits/completes on its own context's owner loop when
started from a worker. Pool callbacks must not block waiting for that owner thread.

Schedulers advertise conservative `weakly_parallel` progress and a success completion scheduler. Error/stopped
affinity is not promised; submission failure may be delivered on the submitting thread.

Use ordinary `then` for synchronous values and `let_value` for dependent senders. `SC::tryThen` is the fallible
synchronous bridge: a callable returning `Result` produces no success values; a callable returning
`AsyncExecutionValueResult<T>` produces one owned T. Callables must be `noexcept`, and T must support nonthrowing
move construction. Reference payloads are rejected. The bridge does not schedule work itself.

`tryThen` checks stop before invoking work. A running procedure cannot be preempted; failure wins over a
concurrent stop, and successful work observes stop after returning. The earlier experimental
`AsyncExecutionBufferResult` and checked-carrier `sendTCP` overloads were removed; use `tryThen` before I/O.

# TCP operations

| Factory | Successful value / contract |
| --- | --- |
| `accept(listener)` | Owned `AsyncExecutionConnection` with associated nonblocking socket |
| `connect(socket, address)` | No value; borrows an existing associated socket; IP/general address overloads |
| `send(socket, data)` | Byte count from one scalar/vector transfer; may be short |
| `sendAll(socket, data[, progress])` | Complete byte count; scalar/vector, skips empty entries and retries |
| `receive(socket, buffer)` | `AsyncExecutionReadResult {data, endOfFile}`; may be short |
| `receiveExact(socket, buffer[, progress])` | Filled span or `SocketReceiveIncomplete` on early disconnect |
| `receiveLine(socket, buffer[, progress])` | `AsyncExecutionLineResult {line, disconnected, lineComplete}` |

Use `loop.createAsyncTCPSocket` or the external-socket association API before borrowed connect/send/receive.
`accept` owns its accepted descriptor; other operations borrow descriptors. Stop never closes a borrowed socket;
after a connect/stop race, close it before retrying if its native connection state is uncertain.

Send/receive counts use the native 32-bit buffer domain. Complete sends use chunks of at most 16 KiB.
Empty complete transfers succeed without I/O. Successful zero progress without disconnect becomes
`SocketReceiveNoProgress` for exact receive. Returned spans alias caller buffers.

`receiveLine` consumes newline, trims a preceding CR only on a complete line and does not read ahead.
Partial EOF succeeds with `disconnected=true`, `lineComplete=false`. Empty storage fails with `EmptyReceiveBuffer`.
A full buffer followed by newline succeeds; an additional ordinary byte is consumed and reports
`ReceiveLineBufferExhausted`, preserving the stored prefix. Reserve capacity for CR when accepting CRLF.

TCP and file reads share `AsyncExecutionReadResult`. Socket EOF means orderly peer closure; file EOF is an
observation of the current range/end and does not promise the file cannot grow.

# File operations

| Factory | Contract |
| --- | --- |
| `fileRead`, `fileReadAt` | One read; emits span and EOF |
| `fileReadExact[At]` | Fills the requested span or reports `UnexpectedEndOfFile` |
| `fileReadUntilFullOrEOF` | Fills the span or succeeds with a final prefix and EOF |
| `fileWrite`, `fileWriteAt` | One scalar/vector write; emits actual byte count |
| `fileWriteAll[At]` | Completes all scalar/vector input, skipping empty entries |
| `fileSend(file, socket, options[, progress])` | Sends one explicit range; emits bytes, mechanism and completeness |

`AsyncExecutionFileOptions` contains borrowed `threadPool`, `offset/useOffset` and `threadPoolMode`.
`NativePreferred` selects native support when available; `ForceThreadPool` requires a pool. Pipes normally use
no pool or offset and must be nonblocking/associated with the loop. Default addressing delegates to Async;
use At-style explicit ranges for portable independent regular-file operations. Legacy explicit-pool overloads
retain absolute-offset default zero.

One-shot sizes fit 32 bits; explicit ranges fit signed 64-bit offsets. Complete transfers use at most 16 KiB
chunks and advance explicit offsets by completed bytes. Empty complete transfers succeed without I/O;
one-shot requests retain Async's `EmptyBuffer` behavior. Exact/fill reads reject zero progress without EOF.

`AsyncExecutionFileSendOptions` supplies offset/length, pipe-size hint and optional pool/mode. Its range is
nonnegative, nonempty and at most 2,147,483,646 bytes; compose ranges for larger files. NativePreferred selects
sendfile, staged Linux splice or Windows TransmitFile. `usedZeroCopy` identifies the mechanism for an observed
prefix, not a physical copying guarantee. Windows forced-worker fallback uses copied I/O on a blocking file
and changes its cursor; independent concurrent readers need separate descriptors.

Early EOF before the requested file-send range completes reports `AsyncError::FileSendIncomplete` with known
prefix in optional progress. Native failures keep their identity. This corrects earlier POSIX zero-progress
retrying and incomplete Windows worker success; it affects all Async adapters.

# Filesystem handles

`fsOpen(pool, path, mode[, poolMode])` copies/resolves the path at sender construction and emits an owning
`AsyncExecutionFile` with a `file` descriptor. Use standard `let_value` to keep it alive through child work,
or `runInto` to transfer ownership to an existing object. Native path storage has StringPath's capacity;
conversion/capacity failures are initialization errors. `fsClose(pool, file[, poolMode])` emits no values and
invalidates the descriptor after a completed close attempt, including a native close failure. Validation and
pool submission failures preserve ownership. Explicit close reports errors; ordinary descriptor RAII remains
available when close errors do not need observation.

`fsRead(pool, file, buffer[, offset, poolMode])` emits the read span and EOF flag;
`fsWrite(pool, file, data[, offset, poolMode])` emits bytes written. Both represent one native operation,
retain native EmptyBuffer/partial-transfer behavior and use an explicit offset (default zero). Buffers,
descriptors and the required pool remain borrowed through completion. Single-transfer/offset bounds match
other file senders. These are seekable filesystem transfers, not the nonblocking-pipe path. Use handles
compatible with the selected backend; filesystem worker fallback uses synchronous FileDescriptor I/O.
In particular, Windows fallback seeks the shared descriptor before reading/writing: serialize those operations
or use independently opened descriptors, and do not assume an unchanged cursor or atomic seek-plus-transfer.
Handles from `fsOpen` are the natural fit. Prefer `fileReadAt`/`fileWriteAt` and their complete-transfer helpers
for ordinary borrowed regular-file ranges; those use the AsyncFileRead/Write request family and its file options.

These factories use AsyncFileSystemOperation: Linux io_uring can perform them natively; other backends use
the supplied pool. `ForceThreadPool` explicitly selects the fallback. After submission, stop joins normal
completion instead of abandoning an internal worker or cancelling away an opened-handle result. A stopped
open closes its unused handle before terminal delivery; a cleanup failure becomes an error. Earlier native
errors retain precedence. Close/write side effects can occur despite stopped delivery; cancellation is not
rollback. Prestop prevents submission and preserves resources.

`fsCopyFile` and `fsCopyDirectory` take `(pool, path, destination[, flags, poolMode])` and emit no values.
They retain FileSystemCopyFlags, including overwrite (off by default) and native-clone preference.
`fsRename(pool, path, destination[, poolMode])`, `fsRemoveFile(pool, path[, poolMode])` and
`fsRemoveEmptyDirectory(pool, path[, poolMode])` also emit no values. The latter requires an empty directory.
Both paths and copy flags are captured at construction. Copy operations use the native pool fallback on all
backends; rename/remove can use io_uring natively. They share the same joined-stop policy and preserve native
errors; cancellation does not undo a completed copy, rename or removal.

For independent copies, use ordinary provider composition (with an initialized context and pool):

```cpp
SC_TRY(context.run(ex::when_all(
    context.fsCopyFile(pool, "message.txt", "copy-a.txt"),
    context.fsCopyFile(pool, "message.txt", "copy-b.txt"))));
```

With default copy flags, both destinations must be absent. The joined graph keeps its copied paths alive
through completion; callers only retain the context and pool.

# Readable readiness

`filePoll(file)` borrows the descriptor and emits no success values. It observes one readable-ready indication
without reading bytes or automatically rearming. Start another sender to observe readiness again. EOF can also
make a descriptor readable; the subsequent read determines data/EOF. Readiness is a hint when other consumers
can drain the descriptor before your read.

The operation uses AsyncFileReadiness with the same owner-thread cancellation and terminal quiescence rules as
other borrowed I/O. Valid Windows handles retain `AsyncError::OperationUnsupported`; no timer-polling emulation
is supplied. Invalid descriptors report `AsyncError::InvalidFileHandle`; prestop avoids validation/submission.
The descriptor must outlive terminal completion, even when a terminal receiver releases connected storage.

# Cross-thread notifications

`AsyncExecutionNotification notification{context}` supplies a stable address for producers.
`context.wakeUp(notification)` is a lazy one-shot sender with no success values. From any thread,
`notification.notify()` publishes a coalescing notification and returns `Result`. Repeated publications merge
into one pending bit, including before the wait starts. This is an SC extension using the context's native
AsyncLoopWakeUp dispatcher. It does not allocate another request or block a worker.

Only one active waiter is allowed (`NotificationAlreadyWaiting`); a notification belongs to its construction
context (`NotificationContextMismatch`). Prestop does not consume a pending bit. Cancellation before dispatch
leaves the bit for the next wait; if dispatch already selected it, the bit is consumed even when stop wins
terminal delivery. A failed `notify()` retains the bit but requires the caller to handle/retry publication.
Success means published, not delivered. Terminal delivery happens on the owner thread and may release connected
storage. Complete the wait and join producers before destroying the notification or closing its context.

# External producer completion

Create `AsyncExecutionExternalCompletion completion{context}` as a stable, single-use record for an external
job. The producer calls `completion.complete(result, bytes)` after its last access to graph-borrowed data.
`context.externalCompletion(completion)` waits for that acknowledgement, emitting the byte count on success
or the unchanged producer `Result` on failure. Publication before the wait is retained. Duplicate publication
reports `AsyncError::CompletionAlreadyPosted`; overlapping waits, wrong context and repeated consumption
report distinct AsyncExecution errors. A failed publication may be retried; a successful one must not be.

Stop sets `completion.isStopRequested()` for a cooperative producer, but waits for acknowledgement even with
an already-stopped token. An external job may already be running when the sender starts. After acknowledgement,
producer error wins over stop; otherwise the sender emits stopped or the byte count. A composed `timeout` also
joins acknowledgement before returning. Producers that never acknowledge cannot be forcibly joined.

This portable callback bridge uses the native wake-up dispatcher; it does not submit Windows OVERLAPPED
operations or provide cancellation for arbitrary third-party APIs. Delivery occurs on the owner thread and
may free connected storage. Join producer threads before destroying the record or closing its context.

# Datagrams, processes and signals

| Factory | Successful value / contract |
| --- | --- |
| `sendTo(socket, address, data)` | One scalar/vector message; byte count; no chunk/retry into another message |
| `receiveFrom(socket, buffer)` | Data prefix, copied IP/general peer address and `truncated` flag |
| `processExit(handle[, progress])` | `AsyncExecutionProcessExitResult {exitStatus, complete}` |
| `signal(number[, options])` | `AsyncExecutionSignalResult {signalNumber, deliveryCount}` for one delivery |

Datagram addresses are copied; sockets, payloads and vector metadata stay borrowed. An empty vector sends one
empty message. Short receives succeed with a prefix and `truncated=true`; excess bytes are discarded, not
returned by the next receive. The original message length is not reported. A zero-capacity receive can consume
a nonempty message as an empty truncated prefix. Check truncation before parsing complete-message protocols.
Windows `WSAEMSGSIZE` follows this prefix-success contract; other errors remain errors. This is an intentional
change from the earlier native Windows error behavior.

Process waits borrow already-launched handles. Nonzero exit status is a value. A successful POSIX wait reaps
the child; stopping never kills it. A composed graph may discard the value after native reaping: optional
progress records `complete=true`, so callers must not wait again. If a pending watch stopped with complete=false,
the caller still owns waiting/reaping. Use one waiter per child and retain its handle owner through completion.

Signal senders do not rearm automatically. Delivery count may reflect coalescing. Use one watcher per process
signal and fan out above it; thread-directed delivery is not portable. Windows retains console-control mappings
2/21/15, console/process-group requirements and native unsupported-signal errors.

@anchor asyncexecution-timeout
# Joined timeout and cancellation

`timeout(context, sender, duration)` starts a relative timer with the work, stops the other branch when either
finishes and joins both before exposing completion. It works without counting scopes and keeps owned, decayed
snapshots of success values, including move-only/multiple values, until destruction. References are copied.
Storage construction must be nonthrowing; timeout itself adds no allocation.

After joining, precedence is: failing work Result, failing timer Result, caller stop, winning deadline
(`TimedOut`), work stopped, work values. Work/deadline success follows first observation; simultaneous ties
have no fixed winner. A timer cancelled by successful work does not report timeout. Completion runs on the
final joined branch's thread; use scheduler composition if you need specific affinity.

Native start/stop is dispatched on the context owner. Active worker functions cannot be preempted;
cancellation waits for completion. Terminal stopped delivery means native requests are quiescent. Progress
reports observed chunks, not an acknowledgment of every possible OS side effect during a cancellation race.
It resets at dispatched start, including prestopped starts, and remains borrowed until completion.

Await/Fibers use their own cancellation Results. Their owner-thread cancellation paths may suppress an already
collected error before dispatch. Sender stop posts an owner command: if that collected error is dispatched
first, its original Result wins; if cancellation is processed first, the callback may be suppressed and stopped
delivered. These delivery windows are deliberately not claimed to have identical precedence.

# Structured scopes and shutdown

`AsyncExecutionScope` borrows a context and owns connected child storage using provider counting-scope
associations. Default nodes allocate with proper alignment and report `OutOfMemory` without starting a child.
Optional `AsyncExecutionScopeAllocator` callbacks receive size/alignment and matching deallocation; allocator
user data survives join. This is no general allocation guarantee for arbitrary provider algorithms.

For related concurrent branches, `when_all(a, b)` requests sibling cancellation after failure and joins both.
Separate `scope.spawn(a)` / `scope.spawn(b)` roots instead share lifetime ownership with independent failure
policies: a child error alone does not request sibling stop.

`spawn` reports acceptance and may execute synchronous senders inline. Schedule CPU work explicitly.
Child values are discarded; consume them within each graph. Child errors must be failing Results and do not
automatically stop siblings. Summary counts accepted/succeeded/failed/stopped roots and retains first observed
error. Starting `join()` seals roots; caller stop requests cancellation but still joins every child and pool
notification. Join returns the summary or the first child error. A provider join error takes precedence;
`getSummary()` retains child observations even when join reports error.

Do not propagate a later spawn rejection with `SC_TRY` while earlier accepted work remains unjoined. Keep the
submission Result, request stop, then join before releasing a used local scope or its borrowed storage. Keep
submission and join Results separately observable: the tutorial/example choose the submission error first,
otherwise the join error. This is an application policy, not scope error aggregation. A rejected submission is
not an accepted/failed child in the summary. Owner-thread consumers must be called outside callbacks and
without an overlapping consumer; external-loop applications retain persistent shutdown state and pump to
join completion. A synchronous scope-exit guard cannot safely provide arbitrary asynchronous joining.

`requestStop()` seals roots and cooperatively stops children. Concurrent join consumers report `AlreadyRunning`;
that failure does not establish quiescence. After join, new roots report `ScopeClosed`; scopes cannot reopen.
Destroy used scopes only after join and producer-thread shutdown. Active destruction fails explicitly rather
than blocking the owner loop. An empty never-used scope needs no join, including failed first allocation.

`AsyncExecution::close()` rejects active operations, delivery, scopes and pool notifications with
`ActiveRequestsRemain`; rejection leaves the runtime usable. Foreign-thread close reports `InvalidState`.
An empty/already closed runtime closes successfully and can be created again. Shutdown proceeds through
dispatcher, loop and pool, returning cleanup failures unchanged and retaining unfinished stages for retry.
Once dispatcher shutdown begins, new starts/runs are rejected even if another cleanup stage fails. Direct native
requests stay caller-managed. Call close explicitly to observe errors; destruction requires joined work on owner.

Pool operations embed their task nodes. `waitForTask` joins node release; `waitForAllTasks`/pool destruction
also join active notifications. Pool destruction discards queued tasks without notification, so it cannot cancel
live graphs. The pool/context survive notification return; scope join includes that tail before runtime close.

@anchor asyncexecution-driving
# Driving an existing loop

Create an `AsyncExecutionContext` over an initialized loop and close it before closing the loop. Its counted
dispatcher keeps `loop.run()` alive. For a manual connect/start driver, keep the immovable connected operation
alive through its receiver's terminal notification, and have a terminal receiver interrupt a blocking run.
For frame polling with `runOnce`/`runNoWait`, track completion and stop polling yourself; no interrupt is needed.
The phased submit/poll/dispatch Async API remains available; submission/dispatch stay on the owner thread.

An external `sync_wait` can consume SC senders only if another thread drives the SC loop. Calling it on the
only loop-driving thread prevents progress. Its standard error/exception policy can terminate on error when
exceptions are disabled; use SC consumers for Result-based failure handling.

Borrowed buffers, descriptors, metadata, contexts and execution resources survive terminal quiescence.
Terminal delivery may destroy connected operation storage. Stop/join producers before closing execution resources.

# Relationship to Await and AsyncFibers

All three borrow Async; they have no adapter-to-adapter dependency. Fibers suspends a stackful task; Await
returns awaitables and uses supplied frame storage; AsyncExecution returns lazy senders with value/error/stopped
channels. Similar operation names preserve resource semantics, not identical result spelling.

| Operation | AsyncFibers | Await | AsyncExecution context |
| --- | --- | --- | --- |
| Timer / TCP basics | Yes | Yes | Yes |
| Scatter/gather TCP | Missing | Yes | Yes |
| Exact/line TCP receive | Missing | Yes | Yes |
| File read/write and explicit offsets | Yes | Yes | Yes |
| Fill-to-EOF / exact file read | Exact | Fill-to-EOF | Both |
| File send, UDP, process, one-shot signal | Yes | Yes | Yes |
| File readiness | `filePoll` | `filePoll` | `filePoll`; POSIX, explicit Windows unsupported error |
| Public wake-up | Internal only | `wakeUp` | One-shot `wakeUp(notification)`; coalescing |
| External-completion wrapper | Missing | Missing | Single-use `externalCompletion`; joins producer acknowledgement |
| Filesystem operations | Missing | `fsOpen/Close/Read/Write/Copy/Rename/Remove` | `fsOpen/Close/Read/Write/CopyFile/CopyDirectory/Rename/RemoveFile/RemoveEmptyDirectory` |
| Lifetime helpers | Scheduler/counters/cancelAll | Task groups/registry/waitFor | Scope spawn/stop/join and joined timeout |

Do not infer complete-transfer behavior from names across adapters: current Fibers `fileWriteAll` delegates
to its write implementation; stronger parity needs separate validation. Process reaping and optional progress
remain native observations even when a composed root discards success values.

# Await interoperability

Await and AsyncExecution can be configured with the same native loop. The owner must drive Await deferred-frame
cleanup, join both adapters and account for the execution dispatcher keeping native work alive.
Direct sender-to-AwaitTask and reverse conversion are deferred:
it needs explicit allocator/frame ownership, owner-thread resumption and joined stop/completion hooks.
There is no implicit `co_await` conversion or provider dependency in Await. The operation parity table above
compares independent adapters; it does not imply that their task types can be interchanged.

# Limits and follow-ups

- Native C++26 provider validation requires an installed implementation exposing the sender and, for scopes,
  counting-scope feature macros. External stdexec test results establish interoperability with that provider.
- Direct Await task conversion is deferred. A future optional bridge needs explicit frame allocation, task
  ownership, owner-loop resumption and cancellation acknowledgement before either adapter can release storage.
- Windows file readiness retains Async's unsupported result. Direct Windows OVERLAPPED submission also remains
  outside the portable external-producer bridge; adding either requires a native lifetime/cancellation contract.
- Cancellation cannot preempt a running CPU procedure or undo filesystem side effects. External producers must
  acknowledge completion. A timeout joins its losing branch and can therefore complete after its deadline.
- The parity table describes the current independent adapters. Stronger Fibers complete-write semantics and
  shared-loop Await/Execution task composition need their own implementation and validation.

These are bounded follow-ups to the experimental adapter. APIs expose the available native semantics;
applications retain the resource and producer lifetimes described above.

# Build settings

Compile `AsyncExecution.cpp` alongside your other sources. Templates/layouts stay in the header; non-template
runtime, dispatcher, allocation and I/O code compiles once. Dedicated targets and `SC.cpp` include it already;
regular/standalone amalgamations use their usual implementation guards. Enabled consumers and implementation
must agree on `SC_INCLUDE_STD_CPP` and provider settings. Disabled integration yields an empty implementation.

Native support requires `__cpp_lib_senders >= 202506L`. Explicit stdexec selection uses
`SC_ASYNC_EXECUTION_USE_STDEXEC=1`; `SC::AsyncExecutionProtocol` aliases the selected provider without injecting
names into `std`. The dedicated targets set this from `SC_ASYNC_EXECUTION_STDEXEC_INCLUDE` and do not download it.
They allow standard headers while keeping exceptions/RTTI disabled; strict SCTest excludes these optional tests.

Scopes are gated separately by `SC_ASYNC_EXECUTION_SCOPES_AVAILABLE`. Native support also needs
`__cpp_lib_counting_scope >= 202506L`; the tested stdexec provider enables scopes explicitly.
Define `SC_ASYNC_EXECUTION_ENABLE_SCOPES=0` consistently to disable scopes without disabling base senders.

```sh
export SC_ASYNC_EXECUTION_STDEXEC_INCLUDE=/absolute/path/to/stdexec/include
./SC.sh build configure SCAsyncExecutionTest
./SC.sh build compile SCAsyncExecutionTest Debug
./SC.sh build run SCAsyncExecutionTest Debug
```

Repeat compile/run with Release for both configurations. On Windows Git Bash also set:

```sh
export CL='/Zc:__cplusplus /Zc:preprocessor /external:anglebrackets /external:W0'
```

These flags supply the MSVC language macro/conforming preprocessor and suppress external-provider warnings.
Use a repository-relative include directory or a Windows path. Provider interoperability is distinct from native
C++26 standard-library conformance; dated runs and exact provider revisions belong in the development evidence log.
