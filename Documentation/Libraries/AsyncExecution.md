# AsyncExecution

Experimental sender/receiver adapter over SC::Async. Requires C++20 language support and an execution implementation matching the relevant WG21 N5046 protocols. N5046 is a working draft, not a published ISO standard. It exposes event-loop and pool schedulers and uses the provider's standard sender algorithms. An owning facade intentionally permits allocations, as authorized for this experiment.

Dependencies: Async. Standard execution integration is optional. With `SC_INCLUDE_STD_CPP=0`, or a standard library without `__cpp_lib_senders >= 202506L`, `SC_ASYNC_EXECUTION_AVAILABLE` is zero and no adapter API is exposed.

## Owning facade

```cpp
SC::AsyncExecution app;
SC_TRY(app.create(2));
namespace ex = SC::AsyncExecutionProtocol;

auto sendEncoded = [&](SC::StringSpan file, uint16_t port)
{
    return app.readFile(file)
        | ex::continues_on(app.getPoolScheduler())
        | ex::then(base64Encode)
        | ex::let_value([&, port](auto& encoded) noexcept {
              return app.sendTCP("127.0.0.1", port, SC::move(encoded));
          });
};
SC_TRY(app.run(ex::when_all(sendEncoded("first.txt", 19092),
                          sendEncoded("second.txt", 19093))));
```

The example codec is in `Examples/AsyncExecutionDemo/Base64.h`. It returns an AsyncExecutionBufferResult containing status and owned data. Standard then treats this as a value; sendTCP explicitly converts a failed status to set_error(Result) before connecting. This avoids exceptions without silently dropping allocation errors. Base64 is an example helper, not a core Async codec dependency.

The runtime owns its event loop, dispatcher and pool. readFile/writeFile copy paths and own their buffers; these convenience operations use blocking file operations on the pool. TCP connect/send use native Async requests and accept numeric IP addresses. sendTCP owns its connection and retries partial sends in chunks of at most 16 KiB. run retains connected storage through terminal completion and returns Result; an optional stop token supports cancellation. Create, run and destroy the runtime on its owner thread; concurrent runs are rejected.

getScheduler returns the event-loop scheduler. getPoolScheduler returns a worker scheduler; successful schedule completion and subsequent then callbacks execute on a worker. Pool callbacks must not block waiting for the runtime's owner thread. A separately allocated task ticket survives operation destruction and is joined before reclamation.

## Borrowing context

Create an AsyncExecutionContext with an existing loop, call context.create(), then context.close() before closing the loop. Its counted dispatcher keeps run alive; a terminal receiver normally interrupts the loop. Each immovable operation owns its native request inline. Borrowed buffers, descriptors and pools must outlive completion.

Factories include sleep, send, sendAll, receive, fileRead, fileWrite and work. The one-shot send/read/write operations may transfer partial data; sendAll retries. read results contain a span and an EOF flag. The low-level file senders prefer native Async I/O where available and take an explicit fallback pool.

Cross-thread start posts to the owner dispatcher. Stop callbacks request owner-thread cancellation of pending native operations; terminal stopped notification waits for quiescence. Active worker functions cannot be preempted; cancellation waits for actual completion. Original operation errors retain their identity. A terminal notification may destroy operation storage. Context/runtime lifetime must extend past all producers and operation completions.

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
