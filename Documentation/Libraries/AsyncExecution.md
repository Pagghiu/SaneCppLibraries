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
`SC::Result`; no exceptions are required. For the next snippets, keep `app` open until their work finishes.

# Join independent work

Use the provider's `when_all` to start independent branches and wait for all of them:

```cpp
auto first  = app.sleep({100}) | ex::then([]() noexcept { return 1; });
auto second = app.sleep({200}) | ex::then([]() noexcept { return 2; });

int a = 0, b = 0;
SC_TRY(app.runInto(ex::when_all(SC::move(first), SC::move(second)), a, b));
// a == 1 and b == 2. Both timers were active together.
```

`runInto` stores successful values in your output objects. Errors and stopped completion leave those outputs
unchanged. Use `run` when you do not need the values.

# Move CPU work to the pool

`on` temporarily switches scheduler, runs the supplied work, then restores the caller's scheduler.
With the SC consumer, that caller is the event loop:

```cpp
int answer = 0;
auto compute = ex::on(app.getPoolScheduler(),
    ex::just(21) | ex::then([](int n) noexcept { return n * 2; }));

SC_TRY(app.runInto(SC::move(compute), answer));
// answer == 42; multiplication ran on a worker.
```

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

SC_TRY(context.run(context.sleep({10})));

SC_TRY(context.close());
SC_TRY(loop.close());
```

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

# Open a file inside a graph

Filesystem senders can own an opened file for the lifetime of a dependent operation:

```cpp
SC::ThreadPool pool;
SC_TRY(pool.create(1));
char buffer[64] = {};
auto readPrefix = context.fsOpen(pool, "message.txt")
    | ex::let_value([&](auto& opened) noexcept {
          return context.fsRead(pool, opened.file, buffer);
      });
SC::AsyncExecutionReadResult prefix;
SC_TRY(context.runInto(SC::move(readPrefix), prefix));
SC_TRY(pool.destroy());
```

The graph owns and closes the file; `prefix.data` aliases your buffer. This reads one prefix, rather than the
whole file. Use `fsClose` inside the graph when close errors need observation. The pool supplies the fallback
on backends without native filesystem operations.

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

Use `AsyncExecutionScope` when connected operation storage must outlive the function that starts work:

```cpp
SC::AsyncExecutionScope scope(app.getContext());
SC_TRY(scope.spawn(ex::when_all(app.sleep({100}), app.sleep({200}))));

// At shutdown, request cancellation and keep driving the loop until join completes.
scope.requestStop();
SC::AsyncExecutionScopeSummary summary;
SC_TRY(app.runInto(scope.join(), summary));
SC_TRY(app.close());
```

`spawn` reports acceptance; `join` reports the children's eventual result after all have finished.
Consume child values inside their graphs. A used scope must be joined before destruction, including after
errors. Scopes need provider counting-scope support in addition to base senders.

For a GUI/server that already drives the loop, connect/start `scope.join()` with your receiver and keep
pumping `runNoWait()` while doing application work. `Examples/AsyncExecutionDemo/Lifetime.cpp` illustrates
this integration. It prepares Base64 data on the pool while connecting, then uses `when_all` for the TCP
exchange. Its 200 ms receive deadline starts after preparation, so CPU queueing does not consume the I/O
budget. A frame processes at most four nonblocking loop cycles; the application must keep pumping until
scope join completes, including after timeout or shutdown. See the
[driving reference](@ref asyncexecution-driving) for receiver and storage lifetimes.

With the execution provider configured, build and run the example:

```sh
./SC.sh build configure SCAsyncExecutionDemo
./SC.sh build compile SCAsyncExecutionDemo Debug
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
