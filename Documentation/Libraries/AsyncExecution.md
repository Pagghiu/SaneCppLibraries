# AsyncExecution

Experimental sender/receiver adapter over SC::Async. Requires C++20 language support and an execution implementation matching the relevant WG21 N5046 protocols. N5046 is a working draft, not a published ISO standard. It exposes event-loop and pool schedulers and uses the provider's standard sender algorithms. An owning facade intentionally permits allocations, as authorized for this experiment.

Dependencies: Async, Strings and Threading. Standard execution integration is optional. With `SC_INCLUDE_STD_CPP=0`, or a standard library without `__cpp_lib_senders >= 202506L`, `SC_ASYNC_EXECUTION_AVAILABLE` is zero and no adapter API is exposed.

## Owning facade

```cpp
SC::AsyncExecution app;
SC_TRY(app.create(2));
namespace ex = SC::AsyncExecutionProtocol;

auto sendEncoded = [&](SC::StringSpan file, uint16_t port)
{
    return app.readFile(file)
        | ex::on(app.getPoolScheduler(), SC::tryThen(base64Encode))
        | ex::let_value([&, port](auto& encoded) noexcept {
              return app.sendTCP("127.0.0.1", port, SC::move(encoded));
          });
};
SC_TRY(app.run(ex::when_all(sendEncoded("first.txt", 19092),
                          sendEncoded("second.txt", 19093))));
```

The example codec is in `Examples/AsyncExecutionDemo/Base64.h`. It returns `AsyncExecutionValueResult<AsyncExecutionBuffer>` containing status and owned bytes. The SC-specific `tryThen` converts its status into execution completion channels before any dependent I/O begins. Standard `on` runs encoding on the pool and restores the loop scheduler afterward. Base64 is an example helper, not a core Async codec dependency.

The runtime owns its event loop, dispatcher and pool. readFile/writeFile copy paths and own their buffers; these convenience operations use blocking file operations on the pool. TCP connect/send use native Async requests and accept numeric IP addresses. sendTCP owns its connection and retries partial sends in chunks of at most 16 KiB. run retains connected storage through terminal completion and returns Result; an optional stop token supports cancellation. Create, run and destroy the runtime on its owner thread; concurrent runs are rejected.

getScheduler returns the event-loop scheduler. getPoolScheduler returns a worker scheduler; successful schedule completion and subsequent then callbacks execute on a worker. Pool callbacks must not block waiting for the runtime's owner thread. A separately allocated task ticket is reclaimed on the worker after ThreadPool releases its node and before receiver delivery. The owner loop never joins a running CPU continuation; pool destruction still joins active release notifications. Borrowed resources must outlive active graphs and producer threads.

## Borrowing context

Create an AsyncExecutionContext with an existing loop, call context.create(), then context.close() before closing the loop. Its counted dispatcher keeps run alive; a terminal receiver normally interrupts the loop. Each immovable operation owns its native request inline. Borrowed buffers, descriptors and pools must outlive completion.

Factories include sleep, accept/connect, send/sendAll (contiguous or scatter/gather), receive/receiveExact/receiveLine, fileRead, fileWrite and work. The one-shot send/read/write operations may transfer partial data; sendAll retries. read results contain a span and an EOF flag. The low-level file senders prefer native Async I/O where available and take an explicit fallback pool.

Cross-thread start posts to the owner dispatcher. Stop callbacks request owner-thread cancellation of pending native operations; terminal stopped notification waits for quiescence. Active worker functions cannot be preempted; cancellation waits for actual completion. Original operation errors retain their identity. A terminal notification may destroy operation storage. Context/runtime lifetime must extend past all producers and operation completions.

## Standard composition and returned values

The SC blocking consumer supplies the loop scheduler through `get_scheduler` and `get_start_scheduler`, and forwards the supplied stop token. This supports both forms of standard `on`: temporary work on another scheduler and restoration to the calling loop. `starts_on` changes where a sender starts; `continues_on` changes where its completion continues. I/O senders still submit and complete on their context's loop, even when started on a worker.

```cpp
int answer = 0;
SC_TRY(app.runInto(
    ex::on(app.getPoolScheduler(),
           ex::just(21) | ex::then([](int n) noexcept { return n * 2; })),
    answer));
// answer == 42; on restored completion to app's loop.
```

`runInto(sender, outputs...)` assigns successful completion arguments, in order, to existing caller-owned objects. It supports multiple values and move-only outputs without allocating a result tuple. Every assignment must be noexcept. The consumer assigns outputs only on success. An operation explicitly given progress storage can update that storage before completion. Do not access output objects from other callbacks or threads until consumption returns. `runIntoWithStop(sender, token, outputs...)` supplies an explicit stop token; `run(sender[, token])` retains the convenient discard-values form. A sender with several possible success signatures must have compatible output assignments for each signature, or normalize its values before consumption.

Both consumers preserve `set_error(SC::Result)` identity and map `set_stopped` to `AsyncExecutionError::Stopped`. Foreign error types must be converted to SC::Result upstream, for example with `let_error`. These are SC consumers with SC's no-exception policy, rather than replacements for standard `sync_wait`. Call from the owner thread outside event-loop callbacks; reentrant consumption is rejected. Consumption keeps operation storage alive until terminal completion, including when unrelated loop interruptions occur.

Both schedulers conservatively advertise `weakly_parallel` progress. Successful scheduling advertises its `set_value` completion scheduler; error and stopped completions do not promise scheduler affinity. Submission failure can be delivered immediately on the submitting thread. No delegation scheduler or custom execution domain is claimed. Native I/O, scheduling and worker senders expose static completion-signature functions with SC::Result errors and a stopped channel.

## Fallible transformations

`SC::tryThen(procedure)` is a sender adaptor closure; `SC::tryThen(sender, procedure)` is its direct form. The callable consumes upstream values as rvalues and must be noexcept. Returning `SC::Result` produces zero values on success; returning `SC::AsyncExecutionValueResult<T>{status, value}` produces one T. T must be an object type with nothrow move construction; reference payloads are rejected. The checked carrier owns its value even when status fails; that payload is discarded on the error path. Move-only values and callables are supported. Use standard `then` for ordinary transformations and `let_value` for dependent asynchronous operations.

The adaptor does not schedule CPU work itself. Compose it with standard `on` for a temporary pool transfer, or `continues_on` to remain on a selected scheduler. It checks the receiver's stop token before invoking the procedure. A running CPU procedure cannot be preempted; a failed Result wins over a concurrent stop request, while successful work observes stop after returning. No downstream value consumer runs on error/stopped.

The same transform can feed different consumers:

```cpp
auto encoded = app.readFile("input.txt")
    | ex::on(app.getPoolScheduler(), SC::tryThen(base64Encode));
SC_TRY(app.run(SC::move(encoded) | ex::let_value([&](auto& bytes) {
    return app.writeFile("encoded.txt", SC::move(bytes));
})));
```

The old `AsyncExecutionBufferResult` and its sendTCP overload remain available for prototype compatibility, but the main path no longer depends on transport-specific unwrapping. Migrate codecs to `AsyncExecutionValueResult<AsyncExecutionBuffer>` and insert `tryThen` in place of `then` when their checked result should select the error channel. Ordinary then continues to pass any checked carrier as a value.

## Driving a borrowed loop

The borrowed context has the same `run`, `runInto` and stop-token consumer forms. `app.getContext()` exposes the owning runtime's context, allowing graph factories to accept either form:

```cpp
auto graph = [](SC::AsyncExecutionContext& io) {
    return io.sleep({10}) | ex::then([]() noexcept { return 42; });
};
SC_TRY(app.runInto(graph(app.getContext()), answer));
// Or, with a context borrowing an already-created loop:
SC_TRY(context.runInto(graph(context), answer));
```

An application's existing GUI/frame driver can instead connect/start the graph and keep its operation state alive, then drive `context.getEventLoop()` using `run`, `runOnce` or `runNoWait`. The counted context dispatcher keeps `run` alive, so a terminal receiver should interrupt it. With `runOnce` or `runNoWait`, track terminal completion yourself and stop polling after completion; avoid setting a loop interrupt solely to terminate such a polling loop. Close the context only after all work and producer threads have joined.

The phased Async driver remains available directly, with caller-provided kernel-event storage:

```cpp
alignas(uint64_t) uint8_t storage[8 * 1024];
SC::AsyncKernelEvents events;
events.eventsMemory = storage;
SC_TRY(loop.submitRequests(events));
SC_TRY(loop.blockingPoll(events));
SC_TRY(loop.dispatchCompletions(events));
```

Submission and completion dispatch stay on the owner thread. As described by AsyncEventLoop, a GUI integration may perform the blocking poll separately and marshal dispatch back to the owner. AsyncExecution adds no second poller.

An external consumer such as `sync_wait` can consume SC senders when another thread actively drives the SC loop. Calling `sync_wait(context.sleep(...))` on the only thread that could drive that loop cannot make progress: the consumer's own wait loop does not drive SC's poller. The interoperability test runs sync_wait on a separate thread and drives the SC loop on its owner. It establishes the success path only. Standard sync_wait uses standard error/exception policy; with exceptions disabled, an error can terminate rather than return SC::Result. Use the SC consumers when Result-based failure handling is required.

## Borrowed TCP operations

`accept(listener)` borrows the listening descriptor and emits an owned `AsyncExecutionConnection`. The accepted socket retains Async's native loop association and is made nonblocking before success. Its empty state and nothrow move assignment support `runInto`; replacing a connection retires its old socket with normal RAII close semantics. Use explicit descriptor close when its failure must be observed.

`connect(socket, SocketIPAddress)` and `connect(socket, SocketAddress)` borrow the caller's socket and emit no value. Create/associate a nonblocking socket first with `context.getEventLoop().createAsyncTCPSocket`, or the existing external-socket association API. Cancellation preserves the caller-owned descriptor; its connection state after a completion/stop race remains subject to the native API, so close it before retrying when necessary. General addresses are passed to Async; no additional address-family support is promised beyond the selected backend. Loopback tests cover IPv4 through both address overloads.

The two scatter/gather overloads borrow the metadata array and all payloads. Metadata contents are preserved after completion; do not inspect or change them while a request is live (the native backend can temporarily adjust and restore an entry). One-shot `send` skips leading empty entries, delegates native scatter transfer and rejects all-empty input with Async EmptyBuffer. Complete `sendAll` skips empty entries, checks aggregate-size arithmetic and retries bounded chunks across vector boundaries. Empty complete transfers succeed with zero bytes without validating a descriptor. Neither complete-transfer overload modifies caller payloads.

`receiveExact` fills the supplied span or emits SocketReceiveIncomplete on early disconnect. Empty spans succeed without I/O; a successful zero-byte completion without disconnect fails with SocketReceiveNoProgress. Successful output is AsyncExecutionReadResult; endOfFile also denotes TCP disconnect.

`receiveLine` returns AsyncExecutionLineResult (`line`, `disconnected`, `lineComplete`). It consumes newline, strips a preceding carriage return only for a complete line, and does not read ahead into the next line. Partial EOF succeeds with disconnected=true and lineComplete=false. Empty storage fails with EmptyReceiveBuffer. A full buffer followed by newline succeeds; an additional ordinary byte is consumed and causes ReceiveLineBufferExhausted, leaving the stored prefix intact. A full partial line at EOF succeeds. These rules match the inspected Await line helper, including the extra capacity needed for a CR before LF.

Optional progress pointers on sendAll/receiveExact/receiveLine expose observed completed chunks on error/stopped, while the standard error channel remains SC::Result. Progress resets when the owner dispatches start, including prestopped starts, rather than when a lazy sender is built. A cancelled in-flight completion may have transferred additional OS bytes that are not represented by progress; the count is not a delivery acknowledgment. Keep progress storage, metadata, descriptors and buffers alive through terminal completion, and inspect progress after completion. Ordinary runInto output assignment remains success-only.

A minimal server graph retains the accepted connection through the dependent operations:

```cpp
char buffer[9];
auto echo = context.accept(listener) | ex::let_value([&](auto& connection) {
    return context.receiveExact(connection.socket, buffer)
        | ex::let_value([&](auto& request) {
              return context.sendAll(connection.socket, request.data);
          });
});
```

The runnable `SCAsyncControlFlowEcho` example supplies the listener, client, watchdog and driver around this graph. It compares callback, fiber, coroutine and sender servers against the same nine-byte native-client exchange. Fibers currently uses an explicit receive loop because that adapter has no receiveExact helper. Each style keeps operation storage alive until success or joined cancellation; `--stall` holds the client after connecting and exercises the two-second watchdog instead of sending the payload.

```sh
export SC_ASYNC_EXECUTION_STDEXEC_INCLUDE=/absolute/path/to/stdexec/include
./SC.sh build configure SCAsyncControlFlowEcho
./SC.sh build compile SCAsyncControlFlowEcho Debug
./SC.sh build run SCAsyncControlFlowEcho Debug -- --port 19400
./SC.sh build run SCAsyncControlFlowEcho Debug -- --port 19400 --stall
```

Windows validation uses the native compile/run path, which resolves the project directly. The generic configure action also exports IDE projects; on the Windows VM it failed during the VS2019 export pass, so successful IDE export is not claimed.

See [the complete four-style example](../../Examples/AsyncControlFlowEcho/AsyncControlFlowEcho.cpp) for resource setup, partial-transfer handling and cancellation. This is a bounded control-flow comparison, not a throughput benchmark.

## Control-flow parity contract and capability matrix

The borrowing adapters are peers over AsyncEventLoop. AsyncFiberIO borrows a FiberScheduler and loop and suspends the current fiber; AwaitEventLoop borrows a loop and frame allocator and returns awaitables; AsyncExecutionContext borrows a loop and returns lazy senders. An owning AsyncExecution additionally owns its loop, pool, paths, buffers and connections. Ownership convenience does not change the borrowing APIs' lifetime requirements.

Inputs should use the same descriptors, spans, address families and operation names when the contracts match. Output spelling intentionally differs: fibers/coroutines return Result and populate output arguments, while senders emit output values through set_value and preserve failure identity through set_error(Result). Sender set_stopped is a separate channel; run maps it to the documented AsyncExecution Stopped error at the synchronous boundary. Do not equate different adapters' cancellation categories or exact-read behavior merely because their methods sound similar.

The following matrix describes implemented APIs and keeps remaining gaps explicit; validation scope is recorded separately:

| Operation / contract | Async primitive | AsyncFibers | Await | AsyncExecution borrowing context |
| --- | --- | --- | --- | --- |
| Relative timer | AsyncLoopTimeout | sleep | sleep | sleep |
| TCP accept / connect | AsyncSocketAccept / Connect | accept / connect (IP address) | accept / connect (IP or general address) | accept emits an owned connection; connect borrows a socket, IP/general addresses |
| One-shot send / receive | AsyncSocketSend / Receive | send / receive | send / receive | send / receive; bytes count or span + end flag |
| Complete TCP send | Repeat AsyncSocketSend | sendAll | sendAll | sendAll, contiguous/scatter buffers, bounded chunks and optional progress |
| Scatter/gather send | AsyncSocketSend buffer spans | Missing | send / sendAll overloads | send / sendAll overloads; metadata and payloads remain borrowed |
| Exact / line receive | Repeat AsyncSocketReceive | Missing | receiveExact / receiveLine | receiveExact / receiveLine, optional progress on error/stopped |
| UDP datagrams | AsyncSocketSendTo / ReceiveFrom | sendTo / receiveFrom, IP addresses | sendTo / receiveFrom, IP/general addresses | Missing |
| One-shot file read | AsyncFileRead | fileRead / fileReadAt | fileRead with options | fileRead; explicit pool, absolute offset default 0 |
| Fill read buffer | Repeat AsyncFileRead | fileReadExact / fileReadExactAt | fileReadUntilFullOrEOF | Missing |
| File write | AsyncFileWrite | fileWrite[At], fileWriteAll[At] | fileWrite, optional options/output and buffer spans | fileWrite; explicit pool, absolute offset default 0 |
| File-to-socket transfer | AsyncFileSend | fileSend | fileSend | Missing |
| File readiness | AsyncFileReadiness | filePoll | filePoll | Missing |
| Process / signal | AsyncProcessExit / AsyncSignal | processExit / signal | processExit / signal | Missing |
| Wake-up / external completion | AsyncLoopWakeUp / AsyncExternalCompletion | Internal command wake-up; no operation API | wakeUp; no external-completion wrapper | Internal dispatcher; no operation API |
| Filesystem operations | AsyncFileSystemOperation | Missing | fsOpen/Close/Read/Write/Copy/Rename/Remove | Missing |
| CPU work | AsyncLoopWork + ThreadPool | No operation method | loopWork | work; owning onPool and pool scheduler |
| Structured lifetime / timeout | Caller-owned requests and stop | FiberScheduler / counters / cancelAll | Task groups/registry, waitFor | Manual connect/start or owning run; no scope/timeout yet |

Current output shapes: Await/Fibers populate send/write byte counts, received spans plus disconnect/EOF state, datagram source addresses, file-send byte counts/zero-copy/completeness, process exit status and signal number/delivery count. Await line reception additionally reports lineComplete. The borrowing sender baseline emits no value for timers/work, size_t for send/write, and AsyncExecutionReadResult (span plus endOfFile, also used for TCP disconnect) for receive/read. Missing sender operations have no published output contract yet; future value types must preserve the corresponding information.

Shared transfer rules: one-shot operations may return short counts; returned read spans alias the supplied buffer. Socket disconnect and file EOF must remain explicit and must not be inferred solely from a short read. Descriptors, request storage and buffers remain valid until terminal notification and underlying Async quiescence. Native sockets must be nonblocking and associated with the borrowed loop.

Important existing differences must be retained until an explicit compatibility decision is made:

- Fibers fileReadExact reports UnexpectedEndOfFile if the buffer cannot be filled. Await fileReadUntilFullOrEOF succeeds with a shorter span and endOfFile. These are different operations, not aliases.
- Await receiveExact errors on disconnect before the requested count and on zero progress. receiveLine returns lineComplete and disconnect state separately. Fibers currently has neither helper.
- AsyncExecution's current file factories always set an absolute offset (default zero) and take an explicit pool. Await options distinguish current position from an explicit offset with useOffset and allow a fallback pool; Fibers uses At-suffixed methods for explicit offsets. Evolution must not silently map default zero to current position.
- Current Fibers fileWriteAll[At] delegates to the same implementation as fileWrite[At]. A stronger complete-transfer contract needs independent verification; the name alone is insufficient evidence.
- Await and Fibers map cancellation into their own Result categories. Await cancellation helpers set the cancelled result before stopping a request and resume after its stop callback; Fibers waitForOperation stops/quiesces the request and then returns its cancellation result. Those active-cancellation paths favor cancellation, while an already completed operation is not retroactively rewritten. Cross-adapter cancellation/error race tests are still needed before claiming fully equivalent precedence. AsyncExecution observes stop before submission, waits for running pool work to finish, and preserves operation errors over stop on failed completion. Cross-layer cancellation precedence remains an explicit comparison point rather than a claim of identical implementation.

Thread affinity and lifetime: native Async submission/completion is driven on the loop owner. Await resumes I/O continuations from that loop; AsyncFibers coordinates execution through its supplied FiberScheduler and optional command storage. AsyncExecution cross-thread start posts to the owner, while successful pool scheduling deliberately delivers its receiver on a pool worker. A subsequent continues_on(loopScheduler) explicitly returns downstream work to the loop. Callers must stop/join producer threads before closing borrowed execution resources.

The owning facade may allocate and provides whole-file/path/connection conveniences. Borrowed operations retain caller ownership and stable operation storage. Sender graphs must remain lazy until start; native Async must be quiescent before buffers or descriptors are reused. Completion may destroy the connected operation. This peer relationship does not add adapter-to-adapter dependencies or force changes to existing Await/Fibers APIs.

## Compilation model

Compile `Libraries/AsyncExecution/AsyncExecution.cpp` alongside the library's other sources. Sender/receiver templates and type layouts remain in the header; dispatcher operations, buffer allocation, file I/O, runtime driving and non-template factories are compiled once in the implementation. The dedicated build targets include it automatically, and `SC.cpp` includes it for single-translation-unit builds.

The implementation and consumers must use the same `SC_INCLUDE_STD_CPP` and execution-provider settings. With integration disabled, the implementation compiles as an empty translation unit. Regular and standalone amalgamated headers include these definitions through their usual implementation guard. `SC_EXPORT_LIBRARY_ASYNCEXECUTION` controls the normal library symbol export convention.

## Reference implementation tests

The default provider is `std::execution`. For toolchains without standard senders, explicitly opt into NVIDIA stdexec using `SC_ASYNC_EXECUTION_USE_STDEXEC=1`; the protocol namespace is then `SC::AsyncExecutionProtocol` (an alias for stdexec). No symbols are injected into std.

Reference provider examined: NVIDIA/stdexec commit `34fa94719f97f49a553811423fa1200172f22a0e`. The test provider is external and not downloaded by the build.

```sh
export SC_ASYNC_EXECUTION_STDEXEC_INCLUDE=/absolute/path/to/stdexec/include
./SC.sh build configure SCAsyncExecutionTest
./SC.sh build compile SCAsyncExecutionTest Debug
./SC.sh build run SCAsyncExecutionTest Debug
./SC.sh build compile SCAsyncExecutionTest Release
./SC.sh build run SCAsyncExecutionTest Release
```

The dedicated target permits standard headers and runtime linkage while retaining disabled exceptions and RTTI. Existing strict SCTest builds do not compile the optional execution tests. Reference-provider checks establish interoperability with that provider; native standard-library conformance remains a separate validation gate.

When running the reference-provider target through Windows Git Bash, also set:

```sh
export CL='/Zc:__cplusplus /Zc:preprocessor /external:anglebrackets /external:W0'
```

The first two flags enable the MSVC language-version macro and conforming preprocessor required by stdexec. The external-header flags suppress warnings from the reference provider and standard headers while preserving warnings in SC code. Supply `SC_ASYNC_EXECUTION_STDEXEC_INCLUDE` as a Windows path (for example, using `cygpath -w`).

## Initial-slice validation (2026-10-17)

The following 53-check results apply to the initial borrowing slice, before the scheduler, prompt cancellation and owning-facade extensions. They are not validation of the current expanded implementation. Current local extension checks are recorded below.

- macOS arm64, Apple Clang 17, reference provider above: Debug and Release each passed 53 assertions across five test sections. Coverage includes `then`/`when_all`, immediate errors, cooperative stop, worker/loop thread affinity, TCP and file transfers, and destruction of caller-provided operation storage during completion.
- Default SCTest suites passed in Debug and Release (opt-in BuildTest excluded).
- Python amalgamation and all SCSingleFileLibs Release builds passed. An additional enabled-reference-provider syntax probe instantiated all six factories from SaneCppAsyncExecutionStandalone.h; this supplements the strict no-standard-header builds, where the optional API is disabled.
- Formatting and diff whitespace checks passed.
- Linux arm64 VM: Debug and Release each passed the same 53 assertions using the reference provider and automatic Async backend selection. This does not separately establish coverage of both forced epoll and forced io_uring modes.
- Windows x64 VM with MSVC and the external-header/conformance flags above: Debug and Release each passed the same 53 assertions. No SC adapter source changes were required.
- Native standard-library sender/receiver execution was unavailable in the installed libc++ headers; it remains a separate validation gate.

## Expanded implementation validation (2026-10-17)

All three platforms passed 319 AsyncExecution assertions per configuration with the pinned stdexec reference provider:

| Platform | Debug | Release | Composed demo and missing-file mode |
| --- | --- | --- | --- |
| macOS arm64 / Apple Clang | 319 passed | 319 passed | Both configurations passed |
| Linux arm64 VM / Clang | 319 passed | 319 passed | Both configurations passed |
| Windows x64 VM / MSVC | 319 passed | 319 passed | Both configurations passed |

Tests cover event-loop/pool scheduler concepts, cross-thread submission, worker/owner affinity, cancellation races, worker quiescence, operation destruction, owning file operations, Base64 vectors and checked encoding errors. The composed demo independently decoded and verified 32768 + 32767 source bytes, receiving 43692 + 43692 Base64 bytes; both encoders ran off the event-loop thread. Missing-file mode verified error propagation and joined sibling cancellation on every platform/configuration.

VM validation exposed a worker cancellation/completion race: the adapter changed a pool request to Cancelling while its normal completion could already be queued. Pool-backed requests now retain normal completion and observe stop after Async joins the worker. Pending native timers and sockets still use request cancellation. Linux UndefinedBehaviorSanitizer also identified null pointers passed to zero-length memcpy/memcmp in the empty-input codec test; that test now skips those calls. The corrected macOS/Linux/Windows runs passed without those diagnostics.

The macOS default SCTest suites passed Debug (35819 assertions) and Release (35921 assertions), with opt-in BuildTest excluded. Amalgamation and all SCSingleFileLibs Release builds passed after the adapter correction. The VM checks above are the dedicated AsyncExecution tests/demo, not full SCTest regressions. Linux used automatic backend selection; this does not independently validate both forced epoll and forced io_uring. Native standard-library sender/receiver validation remains separate from reference-provider interoperability.

The header/implementation split was revalidated with the same 319 assertions and both demo modes in Debug and Release on all three platforms. A two-translation-unit enabled-stdexec standalone-header probe compiled and linked with implementation emitted only once. `SC.cpp` passed a strict C++14/no-standard-C++-headers syntax check with execution disabled and a separate-consumer C++20/stdexec link check with execution enabled. Those probes establish compilation/linkage; runtime validation comes from the dedicated tests and demos above. No build-time speedup was benchmarked.

## Run the composed demo

Use the same reference-provider environment variable as the test target:

```sh
./SC.sh build compile SCAsyncExecutionDemo Debug
./SC.sh build run SCAsyncExecutionDemo Debug
./SC.sh build run SCAsyncExecutionDemo Debug -- --fail
```

The demo creates two input files, starts two loopback fixture peers and joins the transfers with when_all. The normal path checks worker affinity and independently decodes received Base64. The failure mode supplies a missing file, verifies error propagation and joins sibling cancellation. Ports default to 19092/19093; pass `--port N` after `--` to change the pair.

## Evolution checkpoint: composition and TCP parity (2026-10-17)

The M1–M4 evolution passed dedicated tests in Debug/Release on macOS arm64, Linux arm64 and Windows x64/MSVC using the pinned external stdexec provider. The final macOS/Windows dedicated suites passed 780 assertions per configuration; Linux counts varied with polling. Default SCTest Debug/Release, Base64 TCP success/failure demonstrations, and the four-style echo normal/forced-stall runs passed on all three platforms. Opt-in BuildTest was excluded.

Regenerated amalgamation and all single-file Release builds passed. Enabled-provider separate-consumer/implementation linking and enabled/disabled SC.cpp compilation passed; these are compile/link checks. No native standard-library sender conformance, forced Linux backend matrix or Windows IDE export success is claimed. Broader file/UDP/process/signal/readiness/filesystem and structured-scope evolution remains separate work.
