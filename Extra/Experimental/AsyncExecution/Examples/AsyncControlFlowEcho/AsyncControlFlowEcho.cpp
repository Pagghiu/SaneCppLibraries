// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Extra/Experimental/AsyncExecution/AsyncExecution.h"
#include "Libraries/AsyncFibers/AsyncFibers.h"
#include "Libraries/Await/Await.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !SC_ASYNC_EXECUTION_AVAILABLE
#error "Set SC_ASYNC_EXECUTION_STDEXEC_INCLUDE to the reference provider include directory."
#endif

namespace
{
using namespace SC;
namespace ex = AsyncExecutionProtocol;
constexpr ResultCategory EchoCategory{0x80000021u};
enum class EchoError : uint32_t
{
    Timeout = 1,
    Incomplete,
    Mismatch,
    Allocation
};
constexpr char   Payload[] = "same echo";
constexpr size_t Bytes     = sizeof(Payload) - 1;

// All four server styles share this bounded native client and exactly the same payload expectations.
struct Fixture
{
    AsyncEventLoop               loop;
    SocketDescriptor             listener, client;
    SocketIPAddress              address;
    AsyncSocketConnect           connect;
    AsyncSocketSend              send;
    AsyncSocketReceive           receive;
    AsyncLoopTimeout             deadline;
    Function<void(AsyncResult&)> afterClientStopped;
    Function<void()>             cancelServer;
    Result                       status{true};
    char                         reply[Bytes] = {};
    size_t                       sent = 0, received = 0;
    bool                         clientDone = false, serverDone = false, failing = false;
    bool                         stallClient = false;

    Result create(uint16_t port)
    {
        SC_TRY(loop.create());
        SC_TRY(address.fromAddressPort("127.0.0.1", port));
        SC_TRY(loop.createAsyncTCPSocket(address.getAddressFamily(), listener));
        SC_TRY(loop.createAsyncTCPSocket(address.getAddressFamily(), client));
        SC_TRY(SocketServer(listener).bind(address));
        return SocketServer(listener).listen(1);
    }
    bool done() const { return clientDone and serverDone; }
    void checkClientStopped()
    {
        if (connect.isFree() and send.isFree() and receive.isFree())
            clientDone = true;
    }
    void fail(Result error)
    {
        if (status)
            status = error;
        if (failing)
            return;
        failing            = true;
        afterClientStopped = [this](AsyncResult&) { checkClientStopped(); };
        if (not connect.isFree())
            SC_ASYNC_TRUST_RESULT(connect.stop(loop, &afterClientStopped));
        if (not send.isFree())
            SC_ASYNC_TRUST_RESULT(send.stop(loop, &afterClientStopped));
        if (not receive.isFree())
            SC_ASYNC_TRUST_RESULT(receive.stop(loop, &afterClientStopped));
        checkClientStopped();
        cancelServer();
    }
    void finishServer(Result result)
    {
        serverDone = true;
        if (not result)
            fail(result);
    }
    void submit(Result result)
    {
        if (not result)
            fail(result);
    }
    void startClient()
    {
        if (failing)
            return;
        connect.callback = [this](AsyncSocketConnect::Result& result)
        {
            if (not result.isValid())
            {
                fail(result.isValid());
                return;
            }
            if (stallClient)
                return;
            submit(send.start(loop, client, {Payload, Bytes}));
        };
        send.callback = [this](AsyncSocketSend::Result& result)
        {
            if (not result.isValid())
            {
                fail(result.isValid());
                return;
            }
            const size_t count = result.completionData.numBytes;
            if (count == 0 or count > Bytes - sent)
            {
                fail(Result::Error(EchoCategory, EchoError::Incomplete));
                return;
            }
            sent += count;
            if (sent < Bytes)
                submit(send.start(loop, client, {Payload + sent, Bytes - sent}));
            else
                submit(receive.start(loop, client, {reply, Bytes}));
        };
        receive.callback = [this](AsyncSocketReceive::Result& result)
        {
            if (not result.isValid())
            {
                fail(result.isValid());
                return;
            }
            const size_t count = result.completionData.numBytes;
            if (count == 0 or count > Bytes - received)
            {
                fail(Result::Error(EchoCategory, EchoError::Incomplete));
                return;
            }
            received += count;
            if (received < Bytes)
                submit(receive.start(loop, client, {reply + received, Bytes - received}));
            else
                clientDone = true;
        };
        submit(connect.start(loop, client, address));
    }
    void startDeadline()
    {
        deadline.callback = [this](AsyncLoopTimeout::Result&)
        { fail(Result::Error(EchoCategory, EchoError::Timeout)); };
        submit(deadline.start(loop, {2000}));
    }
    Result finish()
    {
        if (not deadline.isFree())
            SC_TRY(deadline.unschedule(loop));
        SC_TRY(client.close());
        SC_TRY(listener.close());
        SC_TRY(loop.close());
        SC_TRY(status);
        return sent == Bytes and received == Bytes and memcmp(reply, Payload, Bytes) == 0
                   ? Result(true)
                   : Result::Error(EchoCategory, EchoError::Mismatch);
    }
};

// Callback control flow: request objects and continuation state remain explicit and caller-owned.
struct CallbackServer
{
    Fixture&                     f;
    SocketDescriptor             peer;
    AsyncSocketAccept            accept;
    AsyncSocketReceive           receive;
    AsyncSocketSend              send;
    Function<void(AsyncResult&)> afterStopped;
    char                         buffer[Bytes] = {};
    size_t                       received = 0, sent = 0;
    bool                         finished = false;

    explicit CallbackServer(Fixture& fixture) : f(fixture) {}
    void finish(Result result)
    {
        if (finished)
            return;
        finished            = true;
        const Result closed = peer.close();
        f.finishServer(result ? closed : result);
    }
    void checkStopped()
    {
        if (accept.isFree() and receive.isFree() and send.isFree())
            finish(Result::Error(EchoCategory, EchoError::Incomplete));
    }
    void cancel()
    {
        afterStopped = [this](AsyncResult&) { checkStopped(); };
        if (not accept.isFree())
            SC_ASYNC_TRUST_RESULT(accept.stop(f.loop, &afterStopped));
        if (not receive.isFree())
            SC_ASYNC_TRUST_RESULT(receive.stop(f.loop, &afterStopped));
        if (not send.isFree())
            SC_ASYNC_TRUST_RESULT(send.stop(f.loop, &afterStopped));
        checkStopped();
    }
    void submit(Result result)
    {
        if (not result)
            finish(result);
    }
    void start()
    {
        accept.callback = [this](AsyncSocketAccept::Result& result)
        {
            Result status = result.moveTo(peer);
            if (status)
                status = peer.setBlocking(false);
            if (not status)
            {
                finish(status);
                return;
            }
            submit(receive.start(f.loop, peer, buffer));
        };
        receive.callback = [this](AsyncSocketReceive::Result& result)
        {
            if (not result.isValid())
            {
                finish(result.isValid());
                return;
            }
            const size_t count = result.completionData.numBytes;
            if (count == 0 or count > Bytes - received)
            {
                finish(Result::Error(EchoCategory, EchoError::Incomplete));
                return;
            }
            received += count;
            if (received < Bytes)
                submit(receive.start(f.loop, peer, {buffer + received, Bytes - received}));
            else
                submit(send.start(f.loop, peer, buffer));
        };
        send.callback = [this](AsyncSocketSend::Result& result)
        {
            if (not result.isValid())
            {
                finish(result.isValid());
                return;
            }
            const size_t count = result.completionData.numBytes;
            if (count == 0 or count > Bytes - sent)
            {
                finish(Result::Error(EchoCategory, EchoError::Incomplete));
                return;
            }
            sent += count;
            if (sent < Bytes)
                submit(send.start(f.loop, peer, {buffer + sent, Bytes - sent}));
            else
                finish(Result(true));
        };
        submit(accept.start(f.loop, f.listener));
    }
};

Result callbacks(Fixture& f)
{
    CallbackServer server{f};
    f.cancelServer = [&server] { server.cancel(); };
    server.start();
    f.startDeadline();
    f.startClient();
    while (not f.done())
        SC_ASYNC_TRUST_RESULT(f.loop.runOnce());
    return f.status;
}

// Fibers lack receiveExact, so preserve the same complete-message contract with an explicit receive loop.
Result fiberServer(AsyncFiberIO& io, Fixture& f)
{
    SocketDescriptor peer;
    char             buffer[Bytes] = {};
    SC_TRY(io.accept(f.listener, peer));
    SC_TRY(peer.setBlocking(false));
    size_t used = 0;
    while (used < Bytes)
    {
        AsyncFiberSocketReceiveResult chunk;
        SC_TRY(io.receive(peer, {buffer + used, Bytes - used}, chunk));
        if (chunk.data.empty())
            return Result::Error(EchoCategory, EchoError::Incomplete);
        used += chunk.data.sizeInBytes();
    }
    return io.sendAll(peer, buffer);
}

Result fibers(Fixture& f)
{
    FiberScheduler scheduler;
    AsyncFiberIO   io{scheduler, f.loop};
    FiberTask      task;
    char           storage[64 * 1024];
    FiberStack     stack{storage};
    f.cancelServer = [&io] { SC_ASYNC_TRUST_RESULT(io.cancelAll()); };
    SC_TRY(scheduler.spawn(task, stack, FiberTask::Procedure([&](FiberScheduler&) { return fiberServer(io, f); })));
    f.startDeadline();
    f.startClient();
    while (not f.done())
    {
        if (not task.isCompleted())
        {
            SC_ASYNC_TRUST_RESULT(io.runOnce());
        }
        else
        {
            SC_ASYNC_TRUST_RESULT(f.loop.runOnce());
        }
        if (task.isCompleted() and not f.serverDone)
            f.finishServer(task.result());
    }
    return f.status;
}

// Coroutine control flow uses the same exact-message contract and ordinary SC descriptor ownership.
AwaitTask coroutineServer(AwaitEventLoop& io, Fixture& f)
{
    SocketDescriptor peer;
    char             buffer[Bytes] = {};
    SC_CO_TRY(co_await io.accept(f.listener, peer));
    SC_CO_TRY(peer.setBlocking(false));
    SC_CO_TRY(co_await io.receiveExact(peer, buffer));
    SC_CO_TRY(co_await io.sendAll(peer, buffer));
    co_return Result(true);
}

Result coroutines(Fixture& f)
{
    char           storage[64 * 1024];
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(storage));
    AwaitEventLoop io{f.loop, allocator};
    AwaitTask      task = coroutineServer(io, f);
    if (not task.isValid())
        return Result::Error(EchoCategory, EchoError::Allocation);
    f.cancelServer = [&]
    {
        if (task.isActive())
            SC_ASYNC_TRUST_RESULT(task.cancel(io));
    };
    SC_TRY(io.spawn(task));
    f.startDeadline();
    f.startClient();
    while (not f.done())
    {
        SC_ASYNC_TRUST_RESULT(io.runOnce());
        if (task.isCompleted() and not f.serverDone)
            f.finishServer(task.result());
    }
    return f.status;
}

#if SC_ASYNC_EXECUTION_USE_STDEXEC
using StopSource = ex::inplace_stop_source;
using StopToken  = ex::inplace_stop_token;
#else
using StopSource = std::inplace_stop_source;
using StopToken  = std::inplace_stop_token;
#endif
struct EchoReceiver
{
    using receiver_concept = ex::receiver_tag;
    Fixture&               f;
    AsyncExecutionContext& context;
    StopToken              token;
    struct Environment
    {
        StopToken               token;
        AsyncExecutionScheduler scheduler;
        auto                    query(ex::get_stop_token_t) const noexcept { return token; }
        auto                    query(ex::get_scheduler_t) const noexcept { return scheduler; }
        auto                    query(ex::get_start_scheduler_t) const noexcept { return scheduler; }
    };
    auto get_env() const noexcept { return Environment{token, context.getScheduler()}; }
    void set_value(size_t) && noexcept { f.finishServer(Result(true)); }
    void set_error(Result result) && noexcept { f.finishServer(result); }
    void set_stopped() && noexcept
    {
        f.finishServer(Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
    }
};

Result senders(Fixture& f)
{
    AsyncExecutionContext io{f.loop};
    SC_TRY(io.create());
    StopSource stop;
    f.cancelServer     = [&stop] { stop.request_stop(); };
    char buffer[Bytes] = {};
    auto echo =
        io.accept(f.listener) |
        ex::let_value(
            [&](auto& connection)
            {
                return io.receiveExact(connection.socket, buffer) |
                       ex::let_value([&](auto& request) { return io.sendAll(connection.socket, request.data); });
            });
    auto operation = ex::connect(move(echo), EchoReceiver{f, io, stop.get_token()});
    ex::start(operation);
    f.startDeadline();
    f.startClient();
    while (not f.done())
        SC_ASYNC_TRUST_RESULT(f.loop.runOnce());
    SC_TRY(io.close());
    return f.status;
}
} // namespace

int main(int argc, char** argv)
{
    SocketNetworking::initNetworking();
    unsigned port  = 19200;
    bool     stall = false;
    for (int index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--stall") == 0)
            stall = true;
        else if (strcmp(argv[index], "--port") == 0 and index + 1 < argc)
            port = static_cast<unsigned>(atoi(argv[++index]));
        else
            return 2;
    }
    if (port < 1024 or port > 65531)
        return 2;
    const char* names[] = {"callbacks", "fibers", "coroutines", "senders"};
    for (int style = 0; style < 4; ++style)
    {
        Fixture fixture;
        fixture.stallClient  = stall;
        const Result created = fixture.create(static_cast<uint16_t>(port + static_cast<unsigned>(style)));
        if (not created)
        {
            (void)fixture.finish();
            printf("setup failed: category %u, error %u\n", created.category().value, created.errorValue());
            return 3;
        }
        const Result status   = style == 0   ? callbacks(fixture)
                                : style == 1 ? fibers(fixture)
                                : style == 2 ? coroutines(fixture)
                                             : senders(fixture);
        const Result finished = fixture.finish();
        if (stall and status.isError(EchoCategory, EchoError::Timeout) and
            finished.isError(EchoCategory, EchoError::Timeout))
        {
            printf("%s: watchdog cancelled and joined pending I/O\n", names[style]);
            continue;
        }
        if (stall or not status or not finished)
        {
            const Result failure = not status ? status : finished;
            printf("%s failed: category %u, error %u\n", names[style], failure.category().value, failure.errorValue());
            return 1;
        }
        printf("%s: verified %zu echoed bytes\n", names[style], Bytes);
    }
    return 0;
}
