@page library_async_execution AsyncExecution

@brief Experimental sender/receiver composition over Async

[TOC]

AsyncExecution composes [Async](@ref library_async) operations using sender/receiver algorithms from the
C++26 working draft WG21 N5050. It uses the same native event loop as Await and AsyncFibers. Native
`std::execution` is the default provider; the optional stdexec backend supports experimentation on older toolchains.
See [setup](@ref asyncexecution-building). These function-body snippets return `SC::Result` and assume resources
remain alive through completion.

# Start and join work

```cpp
namespace ex = SC::AsyncExecutionProtocol;
SC::AsyncExecution app;
SC_TRY(app.create(2)); // Own the loop and two workers.

auto tick = app.sleep({10}) | ex::then([]() noexcept { /* Synchronous continuation. */ });
SC_TRY(app.run(SC::move(tick)));

int a = 0, b = 0;
SC_TRY(app.runInto(ex::when_all(
    app.sleep({10}) | ex::then([]() noexcept { return 1; }),
    app.sleep({20}) | ex::then([]() noexcept { return 2; })), a, b));
```

Senders are lazy: construction starts nothing. `run` starts and joins a graph while driving the owner loop;
`runInto` also assigns successful outputs. Error/stopped leaves output objects unchanged. `when_all` requests
sibling stop after failure and joins every branch. Keep `app` open until all following work is joined, then call
`SC_TRY(app.close())`. Call consumers on the owner thread, outside callbacks and without overlapping consumers.

# Read, encode and send with caller-owned bytes

Include `Examples/AsyncExecutionDemo/Base64.h`. Define this factory at namespace scope:

```cpp
namespace ex = SC::AsyncExecutionProtocol;
struct TransferBuffers {
    char input[4096];
    char encoded[5464];
};
auto makeTransfer(SC::AsyncExecution& app, SC::StringSpan path,
                  TransferBuffers& storage, uint16_t port)
{
    return app.readFile(path, storage.input)
        | ex::on(app.getPoolScheduler(), SC::tryThen(
              [&storage](SC::Span<const char> bytes) noexcept {
                  return base64Encode(bytes, storage.encoded);
              }))
        | ex::let_value([&app, port](SC::Span<const char>& bytes) noexcept {
              return app.sendTCP("127.0.0.1", port, bytes);
          });
}
```

Inside your application function, with `app` created and listening peers on both ports:

```cpp
TransferBuffers storage[2];
SC_TRY(app.run(ex::when_all(
    makeTransfer(app, "first.txt", storage[0], 19092),
    makeTransfer(app, "second.txt", storage[1], 19093))));
```

Each branch uses separate writable arrays. `readFile(path, destination)` reads through observed EOF, repeats
short reads and emits the exact written prefix. Overflow reports `FileTooLarge`; a one-byte EOF probe accepts
exact capacity. Empty capacity only accepts empty files. A failed/stopped read can modify a prefix, and a
concurrently modified file is not a snapshot. Larger inputs need application storage or existing descriptor/chunk APIs.

The codec requires disjoint spans, checks arithmetic/capacity before writing and returns a checked span value.
`tryThen` sends its failing Result to the error channel; ordinary `then` does not unwrap Results. Standard `on`
runs encoding on a worker then restores the caller's scheduler. `continues_on` instead places downstream success
continuations on its scheduler. `app.onPool(callable)` executes the body on a worker and delivers success on the owner.

Use `then` for synchronous values and `let_value` for a dependent sender. **Retaining a span retains no bytes.**
Backing arrays survive every dependent access through joined completion; never return a graph borrowing a local
array. Stop/timeout alone does not permit reuse. Do not move or resize an application container backing an active span.
Large records belong in deliberately provisioned application storage, not copied sender captures or indiscriminate stack arrays.

`sendTCP` owns its temporary connection, borrows bytes and completes the entire send; addresses are numeric.
`writeFile(path, bytes)` writes an entire borrowed span. File conveniences run on the pool and close before
terminal delivery; earlier I/O errors precede close errors. File side effects may survive error/stop. The runnable
demo provides its own peers and checks both streams.

# Borrow an existing loop and descriptors

```cpp
SC::AsyncEventLoop loop;
SC_TRY(loop.create());
SC::AsyncExecutionContext context(loop);
SC_TRY(context.create());
const SC::Result work = context.run(context.sleep({10}));
const SC::Result closed = context.close();
const SC::Result loopClosed = loop.close();
SC_TRY(work);
SC_TRY(closed);
SC_TRY(loopClosed);
```

The context adds a dispatcher to your loop, not another poller. With an open context, an opened regular-file
`file` and live fallback `pool`, a single read into caller storage is:

```cpp
char bytes[64];
SC::AsyncExecutionReadResult prefix;
SC::AsyncExecutionFileOptions options{&pool};
SC_TRY(context.runInto(context.fileReadAt(file, 0, bytes, options), prefix));
```

`fileReadAt` can return a short prefix. `fileReadExactAt` requires the complete range; `fileReadUntilFullOrEOF`
permits EOF before capacity. `fileWriteAllAt` retries partial writes. Prefer explicit offsets for ordinary files.
Socket `receive`/`send` are single transfers; `receiveExact`/`sendAll` complete a range. `fsOpen` supplies a move-only
file value for dependent operations; `fsRead`/`fsWrite` use the filesystem family with offset defaulting to zero.
See the [reference](@ref library_async_execution_reference) for handles, fallback pools and other primitives.

# Keep background roots in typed storage

`run` stores its connected operation locally. For persistent roots use a scope with caller-provided slots of one
exact graph type. With the earlier factory and runtime, this blocking example keeps all storage until join:

```cpp
TransferBuffers payloads[2];
using Transfer = decltype(makeTransfer(app, "", payloads[0], uint16_t{}));
SC::AsyncExecutionScopeSlot<Transfer> slots[2];
SC::AsyncExecutionScopePool<Transfer> pool;
SC_TRY(pool.create(slots));
SC::AsyncExecutionScope scope(app.getContext());

SC::Result submission = scope.spawn(pool, makeTransfer(app, "first.txt", payloads[0], 19092));
if (submission)
    submission = scope.spawn(pool, makeTransfer(app, "second.txt", payloads[1], 19093));
if (not submission)
    scope.requestStop();
const SC::Result joined = app.run(scope.join());
const SC::Result released = pool.close();
// Record submission and joined separately; this application gives submission errors precedence.
SC_TRY(submission);
SC_TRY(joined);
SC_TRY(released);
```

For one graph use `scope.spawn(slot, sender)` directly. Named factories stabilize graph types; different graph
types need separate slots/pools, which can feed the same scope. Slots contain the complete connected operation,
not payloads. Capacity exhaustion reports `ScopeCapacityExhausted`, with no growth or heap spill. Busy or
pool-bound explicit slots report `ScopeStorageInUse`. Rejection never consumes/starts the sender. Pool slots
must outlive the pool; both survive join and producer shutdown. Reclaimed execution storage alone does not
permit reuse of unrelated payloads.

Default scopes configure no node allocator. `spawn(sender)` requires explicit callbacks or the named
`AsyncExecutionScopeAllocator::heap()` constructor argument; this opt-in allocates execution nodes only.
Separate roots have independent error policies: an error does not stop siblings. For related branches use one
`when_all` graph. Join seals the scope and reports the first observed child error after all roots and pool
notification tails finish. A later rejection never undoes earlier acceptance: retain submission errors and always
join a used scope before releasing resources.

Externally driven applications put payloads, slots, pool, scope, error records and the connected join operation
in a stable application owner, then pump `runNoWait`/`runOnce` until terminal completion. A local scope-exit guard
cannot safely join arbitrary asynchronous work. [Lifetime.cpp](../../Examples/AsyncExecutionDemo/Lifetime.cpp)
shows frame-driven cancellation, timeout and shutdown; `--scope-failure` demonstrates retained submission/child errors.

Borrowed requests/tasks are embedded in connected storage. Typed scope storage and payload spans require no
adapter heap allocation. OS resources and arbitrary provider/user algorithms have separate requirements.

@anchor asyncexecution-building
# Build and run

Compile `AsyncExecution.cpp` once, with matching provider settings in consumers and implementation. The native
provider needs `__cpp_lib_senders >= 202506L`; scopes independently need `__cpp_lib_counting_scope >= 202506L`.
The dedicated optional targets accept an external stdexec checkout without downloading it:

```sh
export SC_ASYNC_EXECUTION_STDEXEC_INCLUDE=/absolute/path/to/stdexec/include
./SC.sh build compile SCAsyncExecutionTest Debug
./SC.sh build run SCAsyncExecutionTest Debug
./SC.sh build compile SCAsyncExecutionDemo Debug
./SC.sh build run SCAsyncExecutionDemo Debug
```

Repeat compile/run for Release. Windows setup and provider gates are in the [reference](@ref library_async_execution_reference).
External-provider interoperability is separate from native standard-library conformance.
