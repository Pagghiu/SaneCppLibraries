@page library_async_execution AsyncExecution

@brief Experimental sender/receiver composition over Async

[TOC]

AsyncExecution composes [Async](@ref library_async) timers, sockets, files and background work using C++
sender/receiver algorithms. It shares the same event loop as [Await](@ref library_await) and
[AsyncFibers](@ref library_async_fibers); the difference is how you describe and combine work.

This experiment targets the execution protocols in WG21 N5050, a C++26 working draft. Native `std::execution`
is the default provider; explicit stdexec support lets you try it on toolchains without standard senders.
See [building the examples](@ref asyncexecution-building) for setup.

# Start with a timer

Include `Libraries/AsyncExecution/AsyncExecution.h`. These snippets belong inside functions returning `SC::Result`,
so they can use `SC_TRY` for error propagation.

```cpp
namespace ex = SC::AsyncExecutionProtocol;

SC::AsyncExecution app;
SC_TRY(app.create(2)); // Own an event loop and two worker threads.

auto tick = app.sleep({100})
    | ex::then([]() noexcept { /* Runs after the timer fires. */ });

SC_TRY(app.run(SC::move(tick)));
SC_TRY(app.close());
```

`sleep` creates a **lazy sender**: constructing it starts no timer. `then` attaches synchronous work.
`run` starts the graph, drives the SC event loop and returns when the graph finishes. Failures become
`SC::Result`; no exceptions are required. Later `app` snippets assume a runtime created in the same way and kept open through their work; omit the first snippet's `close()` until the end. Each `run` joins its graph before returning, including on error.

# Join related branches

Use the provider's `when_all` for related concurrent branches that belong to one operation:

```cpp
auto first  = app.sleep({100}) | ex::then([]() noexcept { return 1; });
auto second = app.sleep({200}) | ex::then([]() noexcept { return 2; });

int a = 0, b = 0;
SC_TRY(app.runInto(ex::when_all(SC::move(first), SC::move(second)), a, b));
// a == 1 and b == 2. Both timers were active together.
```

`runInto` stores successful values in your output objects. Errors and stopped completion leave those outputs
unchanged. Use `run` when you do not need the values. If one branch fails, `when_all` requests sibling stop
and waits for every branch before completing. Separate scope roots have a different policy, described below.

# Use your loop and caller-owned storage

You can use the same Async loop that drives callbacks, Await or Fibers. `AsyncExecutionContext` borrows it;
it adds a dispatcher, rather than a second poller. Given an initialized `context`, an opened regular-file
`file` and a live fallback `pool`, read a prefix into your own buffer:

```cpp
char bytes[64] = {};
SC::AsyncExecutionReadResult prefix;
SC::AsyncExecutionFileOptions options{&pool};
SC_TRY(context.runInto(context.fileReadAt(file, 0, bytes, options), prefix));
// prefix.data aliases bytes; this is one read at explicit offset zero, possibly short.
```

The caller owns the descriptor, buffer, pool and context through completion. A pool supplies regular-file fallback;
Async uses native support when available. See the existing-loop setup below and the file decision table before
choosing pipe handles or default addressing. `runInto` is an owner-thread consumer, called outside callbacks
with no other active consumer; an application already driving the loop uses connect/start instead.

Borrowed native-operation connected storage contains its request; pool scheduling embeds its task node.
These paths avoid a separate per-request heap ticket. Owning paths/buffers and whole-file conveniences may
allocate. Scopes allocate connected nodes, with optional caller allocator callbacks. Arbitrary provider algorithms
have their own allocation behavior: the complete library has no blanket no-allocation guarantee.

# Move CPU work to the pool

`on` temporarily switches scheduler, runs the supplied work, then restores the caller's scheduler.
With the SC consumer, that caller is the event loop. Native SC I/O still submits and completes through its
context's owner thread, even when a graph starts on a worker:

```cpp
int answer = 0;
auto compute = ex::on(app.getPoolScheduler(),
    ex::just(21) | ex::then([](int n) noexcept { return n * 2; }));

SC_TRY(app.runInto(SC::move(compute), answer));
// answer == 42; multiplication ran on a worker.
```

To place the following continuation on a worker instead, use `continues_on`:

```cpp
auto placed = ex::just(21)
    | ex::continues_on(app.getPoolScheduler())
    | ex::then([](int n) noexcept { return n * 2; }); // Worker continuation.
SC_TRY(app.run(SC::move(placed)));
```

For a borrowed pool, construct `SC::AsyncExecutionPoolScheduler scheduler{&context, &pool}` and use the same
standard algorithms. Keep both objects alive through worker notification return; after graph completion,
`pool.waitForAllTasks()` or a scope join also joins that tail before resource destruction.
`context.work(pool, procedure)` and `app.onPool(procedure)`
run only the procedure on a worker and deliver success through the owner loop; their downstream `then` runs
there. A pool scheduler delivers successful downstream continuations on a worker. Errors/stopped have no
promised worker affinity. The [scheduling reference](@ref library_async_execution_reference) gives the full contract.

Use `then` when your callable returns a value. Use `let_value` when it returns another sender: the returned
asynchronous operation becomes part of the graph, and its input values stay alive until it completes.

# Read, encode and write a file

The owning runtime provides whole-file conveniences with owned buffers. This pipeline reads a file,
encodes it on a worker, then writes the result:

```cpp
auto encodeFile = app.readFile("input.txt")
    | ex::on(app.getPoolScheduler(), SC::tryThen(base64Encode))
    | ex::let_value([&](auto& encoded) noexcept {
          return app.writeFile("encoded.txt", SC::move(encoded));
      });

SC_TRY(app.run(SC::move(encodeFile)));
```

Include `Examples/AsyncExecutionDemo/Base64.h` for the example's `base64Encode` helper. It returns
`AsyncExecutionValueResult<AsyncExecutionBuffer>`, containing a `Result` and owned bytes.
`SC::tryThen` converts a failing `Result` into the error channel, so the write never starts on encoding failure.
Ordinary `then` does not inspect or unwrap a returned `Result`.

Whole-file reads and writes use the pool. These conveniences may allocate; the borrowing API below lets you
supply your own descriptors and buffers instead.

# Send two encoded files concurrently

Replace the final write with TCP sending, then join two pipelines:

```cpp
auto sendEncoded = [&](SC::StringSpan path, uint16_t port) {
    return app.readFile(path)
        | ex::on(app.getPoolScheduler(), SC::tryThen(base64Encode))
        | ex::let_value([&, port](auto& encoded) noexcept {
              return app.sendTCP("127.0.0.1", port, SC::move(encoded));
          });
};

SC_TRY(app.run(ex::when_all(sendEncoded("first.txt", 19092),
                          sendEncoded("second.txt", 19093))));
```

Start listening peers on those ports before trying this snippet. `sendTCP` owns its connection and buffer and
retries partial writes. Addresses must be numeric IP addresses. The runnable demo supplies its own peers and
verifies the received Base64 data.

# Reuse an existing Async loop

`AsyncExecutionContext` borrows a loop instead of owning a runtime. It returns the same kind of senders:

```cpp
SC::AsyncEventLoop loop;
SC_TRY(loop.create());
SC::AsyncExecutionContext context(loop);
SC_TRY(context.create());

const SC::Result work = context.run(context.sleep({10})); // Joins before returning.
const SC::Result closed = context.close();
const SC::Result loopClosed = loop.close();
SC_TRY(work);
SC_TRY(closed);
SC_TRY(loopClosed);
```

The following context snippets assume this context is kept open; delay these close calls until their graphs
have joined. They state any additional descriptor, pool or peer prerequisites at the point of use.

Borrowing socket and file operations keep resource ownership with you. Given an initialized context,
an associated listening socket `listener`, and an eight-byte caller-owned `buffer`, this accepts one peer
and echoes one message:

```cpp
auto echo = context.accept(listener)
    | ex::let_value([&](auto& peer) noexcept {
          return context.receiveExact(peer.socket, buffer)
              | ex::let_value([&](auto& request) noexcept {
                    return context.sendAll(peer.socket, request.data);
                });
      });

SC_TRY(context.run(SC::move(echo)));
```

`let_value` keeps the accepted connection alive through receive and send. `receiveExact` fills the buffer or
reports incomplete input; `sendAll` handles partial writes. The lower-level `receive` and `send` perform one
transfer and can return a shorter prefix.

# Choose a file-operation family

For an existing regular-file descriptor, prefer explicit `fileReadAt`/`fileWriteAt` and their exact/all helpers.
An explicit range does not depend on an assumed portable shared file cursor.

| Need | Choose | Storage, addressing and execution |
| --- | --- | --- |
| Whole file with owned bytes | `app.readFile` / `writeFile` | Owned path/buffer conveniences; pool work; may allocate |
| One borrowed regular-file transfer | `context.fileReadAt` / `fileWriteAt` | Caller descriptor/buffer; explicit offset; optional pool supplies backend fallback |
| Complete an explicit range | `fileReadExactAt` / `fileWriteAllAt` | Retry partial transfers; explicit offsets advance by actual progress |
| Fill up to EOF | `fileReadUntilFullOrEOF` with `options.useOffset=true` and `options.offset` | Caller storage; explicit range start; may return a final prefix |
| Nonseekable pipe or delegated native addressing | `fileRead` / `fileWrite` | Use appropriate associated/nonblocking handles, no offset for pipes; see reference |
| Open/close a handle, copy/rename/remove paths | `fsOpen` / `fsClose` / `fsCopy...` / `fsRename` / `fsRemove...` | Filesystem request family; required pool for fallback; paths/flags copied |
| One transfer in the filesystem request family | `fsRead` / `fsWrite` | Explicit offset **defaults to zero**; required pool; native io_uring or filesystem worker fallback |

`fsRead` is not a cursor-reading alias for `fileRead`. It uses `AsyncFileSystemOperation`, with an offset even
when omitted. `fileRead` uses `AsyncFileRead`; default options delegate addressing to Async, while `fileReadAt`
sets an explicit offset. Both can return a short prefix. A required `fsRead` pool and a supplied `fileRead`
fallback pool must outlive the operation. `fsRead` is for applicable seekable filesystem handles; it is not the
pipe-read path. Filesystem worker fallback needs handles compatible with synchronous file I/O; Windows fallback
seeks the descriptor, so serialize it or use independent descriptors for concurrent operations. See the [file contracts](@ref library_async_execution_reference) for native-preferred versus forced
pool execution and handle setup. Exact/all behavior comes from the named helpers, not from choosing `fsRead`.

# Open a file inside a graph

Filesystem senders can own an opened file for the lifetime of a dependent operation:

```cpp
SC::ThreadPool pool;
SC_TRY(pool.create(1));
char buffer[64] = {};
SC::AsyncExecutionFileOptions readOptions{&pool};
auto readPrefix = context.fsOpen(pool, "message.txt")
    | ex::let_value([&](auto& opened) noexcept {
          return context.fileReadAt(opened.file, 0, buffer, readOptions);
      });
SC::AsyncExecutionReadResult prefix;
SC_TRY(context.runInto(SC::move(readPrefix), prefix));
SC_TRY(pool.destroy());
```

The graph owns and closes the file; `prefix.data` aliases your buffer. This reads one prefix, rather than the
whole file. Use `fsClose` inside the graph when close errors need observation. The pool supplies the fallback
on backends without native filesystem operations; `readOptions` also supplies regular-file read fallback.
This uses the recommended explicit-offset borrowed read after filesystem open.

# Add a deadline

Given a connected socket `peer` and a borrowed buffer, wrap the receive with the SC `timeout` extension:

```cpp
auto read = SC::timeout(context, context.receiveExact(peer, buffer), {250});
SC::AsyncExecutionReadResult received;
SC_TRY(context.runInto(SC::move(read), received));
```

A winning deadline reports `AsyncExecutionError::TimedOut`. Timeout requests cancellation and **joins both
branches** before completing, so buffer reuse is safe afterwards. Running CPU functions cannot be preempted,
and cleanup can outlast the deadline. See the [timeout rules](@ref asyncexecution-timeout)
for racing errors and stop.

# Own background graphs with a scope

`AsyncExecutionScope` owns connected root storage, while the scope itself and its borrowed resources must
survive join. Choose the graph failure policy separately:

```cpp
// Related branches: either failure requests sibling stop; completion joins both.
SC_TRY(app.run(ex::when_all(app.sleep({100}), app.sleep({200}))));
```

Two `scope.spawn(...)` calls instead create independent lifetime-owned roots. A root error does not stop its
siblings. For related background work, compose the branches with `when_all` before spawning one root.
The application decides when to call `requestStop()` for independent roots. Do not use sequential `SC_TRY(scope.spawn(...))`
when an earlier root may have been accepted: a later failure would return before that used scope is joined.

For an owner-thread blocking consumer, retain the submission error and always join:

```cpp
SC::AsyncExecutionScope scope(app.getContext());
SC::Result submission = scope.spawn(app.sleep({100}));
if (submission)
    submission = scope.spawn(app.sleep({200}));
if (not submission)
    scope.requestStop();

// Keep these separate: join can report an earlier child's error as well.
const SC::Result joined = app.run(scope.join());
const SC::Result closed = app.close();
// Record both submission and joined in your application's error/reporting state here.
SC_TRY(submission); // This example policy gives the original submission error precedence.
SC_TRY(joined);
SC_TRY(closed);
```

Call this consumer on the owner thread, outside callbacks, without an overlapping consumer/join. A successfully
accepted root is not undone by a later rejection. Joining seals further submissions; summary counts only accepted
roots as succeeded/failed/stopped and retains the first observed child error. `spawn` rejection is a separate
submission Result. The runnable `--scope-failure` example injects a second-allocation failure with accepted work
still pending, joins it, verifies immediate buffer/context cleanup, and prints **both** errors when that work fails.
An ordinary synchronous scope-exit guard cannot safely drive arbitrary asynchronous shutdown; keep shutdown
explicit rather than blocking a scope destructor.

`spawn` reports acceptance; `join` reports the children's eventual result after all have finished.
Consume child values inside their graphs. A used scope must be joined before destruction, including after
errors. Scopes need provider counting-scope support in addition to base senders.

For a GUI/server that already drives the loop, keep the scope, borrowed resources, submission error and join
receiver/operation in persistent application state. On submission failure, retain that error, request stop,
start join, and keep processing frames until its terminal notification. Only then report the chosen error and
release state. Returning from the submit function is safe only if this persistent owner remains alive.
Connect/start `scope.join()` with your receiver and keep pumping `runNoWait()` while doing application work. `Examples/AsyncExecutionDemo/Lifetime.cpp` illustrates
this integration. It prepares Base64 data on the pool while connecting, then uses `when_all` for the TCP
exchange. Its 200 ms receive deadline starts after preparation, so CPU queueing does not consume the I/O
budget. A frame processes at most four nonblocking loop cycles; the application must keep pumping until
scope join completes, including after timeout or shutdown. See the
[driving reference](@ref asyncexecution-driving) for receiver and storage lifetimes.

With the execution provider configured, build and run the example:

```sh
./SC.sh build configure SCAsyncExecutionDemo
./SC.sh build compile SCAsyncExecutionDemo Debug
./SC.sh build run SCAsyncExecutionDemo Debug -- --scope-failure
./SC.sh build run SCAsyncExecutionDemo Debug -- --lifetime
./SC.sh build run SCAsyncExecutionDemo Debug -- --lifetime --stall
./SC.sh build run SCAsyncExecutionDemo Debug -- --lifetime --cancel
```

`--stall` withholds client bytes to exercise timeout and joined cancellation. `--cancel` requests scope
stop once the receive starts. Add `--frame-ms 40` to simulate a slower application frame cadence;
this is a responsiveness example, not a scheduling-latency guarantee.

# Choose the operation you need

| Work | Owning runtime | Borrowing context |
| --- | --- | --- |
| Timers and workers | `sleep`, pool scheduler, `onPool` | `sleep`, loop scheduler, `work` |
| TCP | `connectTCP`, `sendTCP` | `accept`, `connect`, `send`, `sendAll`, `receive`, `receiveExact`, `receiveLine` |
| Files | `readFile`, `writeFile` | `fileRead[At]`, `fileReadExact[At]`, `fileReadUntilFullOrEOF`, `fileWrite[All][At]`, `fileSend` |
| Filesystem | Through `getContext()` | `fsOpen/Close/Read/Write`, copy file/directory, rename, remove |
| Readiness and wake-up | Through `getContext()` | One-shot `filePoll` (POSIX), `wakeUp(notification)` |
| Datagrams | Through `getContext()` | `sendTo`, `receiveFrom` |
| External producer | Through `getContext()` | One-shot `externalCompletion(record)` |
| Processes and signals | Through `getContext()` | `processExit`, one-shot `signal` |
| Joined lifetime | `run`, `runInto`, explicit `close` | `run`, `runInto`; `AsyncExecutionScope`, `timeout` |

`filePoll` observes readability without consuming bytes; generic Windows handle readiness reports `OperationUnsupported`. The [reference](@ref library_async_execution_reference) covers partial transfers, EOF, datagram truncation and cancellation.

Borrowed buffers, descriptors, pools and contexts must survive terminal completion. CPU cancellation waits for
running work. Join graphs and producer threads before calling `close()`, which returns cleanup errors and
rejects active adapter work rather than cancelling it implicitly.

@anchor asyncexecution-building
# Build and run the examples

Integration requires `SC_INCLUDE_STD_CPP=1` and a compatible execution provider. Without one,
`SC_ASYNC_EXECUTION_AVAILABLE` is zero. The default is `std::execution`; the current test setup explicitly
selects external NVIDIA stdexec through its include path:

```sh
export SC_ASYNC_EXECUTION_STDEXEC_INCLUDE=/absolute/path/to/stdexec/include
./SC.sh build configure SCAsyncExecutionDemo
./SC.sh build compile SCAsyncExecutionDemo Debug
./SC.sh build run SCAsyncExecutionDemo Debug
./SC.sh build run SCAsyncExecutionDemo Debug -- --fail
```

The build does not download the provider. Normal mode runs the two-file TCP example; `--fail` demonstrates
error propagation and joined sibling cancellation. `--port N` changes its pair of listening ports.
`SCAsyncControlFlowEcho` compares callback, fiber, coroutine and sender versions of the same bounded exchange.

For compilation settings, scope feature gates and the full adapter comparison, see the
[reference](@ref library_async_execution_reference). Provider interoperability tests do not establish native C++26 library
conformance. Dated validation results are kept in the development evidence log.
