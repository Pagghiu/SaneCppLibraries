// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
namespace
{
using namespace SC;
namespace ex = AsyncExecutionProtocol;
struct FrameJoin
{
    using receiver_concept = ex::receiver_tag;
    AsyncExecutionContext& context;
    bool&                  done;
    Result&                status;
    struct Environment
    {
        AsyncExecutionScheduler scheduler;
        auto                    query(ex::get_scheduler_t) const noexcept { return scheduler; }
        auto                    query(ex::get_start_scheduler_t) const noexcept { return scheduler; }
    };
    auto get_env() const noexcept { return Environment{context.getScheduler()}; }
    void set_value(AsyncExecutionScopeSummary) && noexcept { done = true; }
    void set_error(Result error) && noexcept
    {
        status = error;
        done   = true;
    }
};
} // namespace
#endif

SC::Result runAsyncExecutionLifetimeDemo(bool stall, bool cancel, uint16_t port)
{
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
    AsyncExecution app;
    SC_TRY(app.create(2));
    auto& context = app.getContext();
    auto& loop    = context.getEventLoop();
    SocketNetworking::initNetworking();
    SocketIPAddress address;
    SC_TRY(address.fromAddressPort("127.0.0.1", port));
    SocketDescriptor listener, client;
    SC_TRY(loop.createAsyncTCPSocket(address.getAddressFamily(), listener));
    SC_TRY(loop.createAsyncTCPSocket(address.getAddressFamily(), client));
    SC_TRY(SocketServer(listener).bind(address));
    SC_TRY(SocketServer(listener).listen(1));

    char                request[8] = {}, reply[8] = {};
    bool                encodedOnWorker = false;
    const auto          owner           = Thread::CurrentThreadID();
    AsyncExecutionScope scope(context);
    auto                server =
        context.accept(listener) |
        ex::let_value(
            [&](auto& peer) noexcept
            {
                return timeout(context, context.receiveExact(peer.socket, request), {200}) |
                       ex::let_value([&](auto& read) noexcept { return context.sendAll(peer.socket, read.data); });
            });
    auto codec = [&]() noexcept -> AsyncExecutionValueResult<AsyncExecutionBuffer>
    {
        encodedOnWorker = Thread::CurrentThreadID() != owner;
        AsyncExecutionBuffer bytes;
        const Result         allocated = bytes.resize(4);
        if (not allocated)
            return {allocated, {}};
        memcpy(bytes.bytes().data(), "ping", 4);
        return base64Encode(move(bytes));
    };
    auto sender = context.connect(client, address) |
                  ex::let_value(
                      [&]() noexcept
                      {
                          return context.sleep({stall or cancel ? 5000 : 0}) |
                                 ex::on(app.getPoolScheduler(), tryThen(codec)) |
                                 ex::let_value(
                                     [&](auto& encoded) noexcept
                                     {
                                         return context.sendAll(client, encoded.bytes()) |
                                                ex::let_value([&](size_t&) noexcept
                                                              { return context.receiveExact(client, reply); });
                                     });
                      });

    // A single scope owns the fan-out. Buffers, sockets and the runtime outlive its joined completion.
    SC_TRY(scope.spawn(ex::when_all(move(server), move(sender))));
    SC_TRY(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
    bool   done = false;
    Result status{true};
    auto   join = ex::connect(scope.join(), FrameJoin{context, done, status});
    ex::start(join);

    Atomic<bool> finished = false;
    Thread       guard;
    SC_ASYNC_TRUST_RESULT(guard.start(
        [&finished](Thread&)
        {
            for (int i = 0; i < 4000 and not finished.load(); ++i)
                Thread::Sleep(1);
            if (not finished.load())
            {
                fprintf(stderr, "Lifetime demo exceeded its independent progress bound\n");
                ::_Exit(124);
            }
        }));
    unsigned frames = 0;
    while (not done)
    {
        // This is a frame/message-loop pump: runNoWait never waits for a graph or CPU continuation.
        SC_ASYNC_TRUST_RESULT(loop.runNoWait());
        ++frames;
        if (cancel and frames == 5)
            scope.requestStop();
        Thread::Sleep(1); // Stand in for application work between frames.
    }
    finished.store(true);
    SC_TRY(guard.join());
    const auto summary = scope.getSummary();
    if (cancel)
    {
        SC_TRY(status and summary.accepted == 1 and summary.stopped == 1);
        printf("Frame shutdown requested stop and joined native/client cancellation.\n");
    }
    else if (stall)
    {
        SC_TRY(status.isError(AsyncExecutionResultCategory, AsyncExecutionError::TimedOut) and summary.failed == 1);
        printf("Receive deadline stopped and joined both fan-out branches.\n");
    }
    else
    {
        SC_TRY(status and summary.succeeded == 1 and encodedOnWorker);
        SC_TRY(memcmp(request, "cGluZw==", 8) == 0 and memcmp(reply, request, 8) == 0);
        printf("Frames stayed responsive through pool encoding and TCP fan-out.\n");
    }
    // Joined scope also covers pool notification tails. Reuse borrowed buffers and close observably.
    memset(request, '?', sizeof(request));
    memset(reply, '?', sizeof(reply));
    SC_TRY(loop.runNoWait());
    for (char byte : request)
        SC_TRY(byte == '?');
    for (char byte : reply)
        SC_TRY(byte == '?');
    SC_TRY(client.close());
    SC_TRY(listener.close());
    SC_TRY(app.close());
    return Result(true);
#else
    (void)stall;
    (void)cancel;
    (void)port;
    fprintf(stderr, "Lifetime demo requires counting-scope support\n");
    return SC::Result(false);
#endif
}
