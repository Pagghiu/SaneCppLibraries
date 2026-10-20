// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Base64.h"
#include "Libraries/Time/Time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
namespace
{
using namespace SC;
namespace ex = AsyncExecutionProtocol;
constexpr ResultCategory LifetimeCategory{0x80000022u};
enum class LifetimeError : uint32_t
{
    ActiveCloseAccepted = 1,
    UnexpectedSummary,
    UnexpectedPayload,
    BufferChanged
};
Result requireLifetime(bool condition, LifetimeError error)
{
    return condition ? Result(true) : Result::Error(LifetimeCategory, error);
}
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

SC::Result runAsyncExecutionLifetimeDemo(bool stall, bool cancel, uint16_t port, unsigned frameMilliseconds)
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

    char request[8] = {}, reply[8] = {};

    bool encodedOnWorker = false;

    int64_t receiveStarted = 0;

    const auto owner = Thread::CurrentThreadID();

    AsyncExecutionScope scope(context);

    char encodedStorage[8];
    auto codec = [&]() noexcept
    {
        encodedOnWorker = Thread::CurrentThreadID() != owner;
        return base64Encode({"ping", 4}, encodedStorage);
    };

    // Prepare CPU data and the connection concurrently. The receive deadline measures the transfer phase;
    // pool queueing and CPU preparation have no latency guarantee and must not consume that I/O budget.
    auto exchange =
        ex::when_all(ex::just() | ex::on(app.getPoolScheduler(), tryThen(codec)), context.connect(client, address)) |
        ex::let_value(
            [&](auto& encoded) noexcept
            {
                auto server = context.accept(listener) |
                              ex::let_value(
                                  [&](auto& peer) noexcept
                                  {
                                      receiveStarted = Time::Monotonic::now().getMonotonicMilliseconds();
                                      return timeout(context, context.receiveExact(peer.socket, request), {200}) |
                                             ex::let_value([&](auto& read) noexcept
                                                           { return context.sendAll(peer.socket, read.data); });
                                  });
                // Deliberately withhold client bytes for the timeout and shutdown modes.
                auto sender =
                    context.sleep({stall or cancel ? 5000 : 0}) |
                    ex::let_value(
                        [&]() noexcept
                        {
                            return context.sendAll(client, encoded) |
                                   ex::let_value([&](size_t&) noexcept { return context.receiveExact(client, reply); });
                        });
                return ex::when_all(move(server), move(sender));
            });

    // A single scope owns preparation and transfer. Joined completion releases all borrowed resources.
    SC_TRY(scope.spawn(move(exchange)));
    SC_TRY(requireLifetime(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain),
                           LifetimeError::ActiveCloseAccepted));
    bool   done = false;
    Result status{true};
    auto   join = ex::connect(scope.join(), FrameJoin{context, done, status});
    ex::start(join);

    Atomic<bool> finished = false;
    Thread       guard;
    SC_ASYNC_TRUST_RESULT(guard.start(
        [&finished](Thread&)
        {
            const auto started = Time::Monotonic::now().getMonotonicMilliseconds();
            while (not finished.load() and Time::Monotonic::now().getMonotonicMilliseconds() - started < 4000)
                Thread::Sleep(1);
            if (not finished.load())
            {
                fprintf(stderr, "Lifetime demo exceeded its independent progress bound\n");
                ::_Exit(124);
            }
        }));
    bool       stopRequested = false;
    unsigned   frames        = 0;
    const auto started       = Time::Monotonic::now().getMonotonicMilliseconds();
    int64_t    previous = started, maximumGap = 0;
    while (not done)
    {
        const auto now = Time::Monotonic::now().getMonotonicMilliseconds();
        if (now - previous > maximumGap)
            maximumGap = now - previous;
        previous = now;
        // Bound ready-I/O work per frame. Each runNoWait call is one kernel/dispatcher cycle;
        // a continuation that queues its next native request otherwise waits another whole frame.
        for (unsigned pass = 0; pass < 4 and not done; ++pass)
        {
            SC_ASYNC_TRUST_RESULT(loop.runNoWait());
        }
        ++frames;
        if (cancel and receiveStarted != 0 and not stopRequested)
        {
            scope.requestStop();
            stopRequested = true;
        }
        if (not done)
            Thread::Sleep(frameMilliseconds); // Stand in for application work between frames.
    }
    finished.store(true);
    SC_TRY(guard.join());
    const auto summary    = scope.getSummary();
    const auto finishedAt = Time::Monotonic::now().getMonotonicMilliseconds();
    if (not cancel and not stall and (not status or summary.succeeded != 1 or not encodedOnWorker))
        fprintf(stderr,
                "Lifetime normal: status=%u/%u succeeded=%zu failed=%zu stopped=%zu worker=%d frames=%u "
                "elapsed=%lldms receive=%lldms max-gap=%lldms\n",
                status.category().value, status.errorValue(), summary.succeeded, summary.failed, summary.stopped,
                int(encodedOnWorker), frames, static_cast<long long>(finishedAt - started),
                static_cast<long long>(receiveStarted ? finishedAt - receiveStarted : -1),
                static_cast<long long>(maximumGap));

    if (cancel)
    {
        SC_TRY(status);
        SC_TRY(requireLifetime(summary.accepted == 1 and summary.stopped == 1, LifetimeError::UnexpectedSummary));
        printf("Frame shutdown requested stop and joined native/client cancellation.\n");
    }
    else if (stall)
    {
        SC_TRY(requireLifetime(status.isError(AsyncExecutionResultCategory, AsyncExecutionError::TimedOut) and
                                   summary.failed == 1,
                               LifetimeError::UnexpectedSummary));
        printf("Receive deadline stopped and joined both fan-out branches.\n");
    }
    else
    {
        SC_TRY(status);
        SC_TRY(requireLifetime(summary.succeeded == 1 and encodedOnWorker, LifetimeError::UnexpectedSummary));
        SC_TRY(requireLifetime(memcmp(request, "cGluZw==", 8) == 0 and memcmp(reply, request, 8) == 0,
                               LifetimeError::UnexpectedPayload));
        printf("Frames stayed responsive through pool encoding and TCP fan-out: %u frames, %lld ms, max gap %lld ms.\n",
               frames, static_cast<long long>(finishedAt - started), static_cast<long long>(maximumGap));
    }
    // Joined scope also covers pool notification tails. Reuse borrowed buffers and close observably.
    memset(request, '?', sizeof(request));
    memset(reply, '?', sizeof(reply));
    SC_TRY(loop.runNoWait());
    for (char byte : request)
        SC_TRY(requireLifetime(byte == '?', LifetimeError::BufferChanged));
    for (char byte : reply)
        SC_TRY(requireLifetime(byte == '?', LifetimeError::BufferChanged));
    SC_TRY(client.close());
    SC_TRY(listener.close());
    SC_TRY(app.close());
    return Result(true);
#else
    (void)stall;
    (void)cancel;
    (void)port;
    (void)frameMilliseconds;
    fprintf(stderr, "Lifetime demo requires counting-scope support\n");
    return SC::Result(false);
#endif
}
