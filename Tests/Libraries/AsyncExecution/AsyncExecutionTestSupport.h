// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/AsyncExecution/AsyncExecution.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
namespace
{
namespace ex = AsyncExecutionProtocol;
#if SC_ASYNC_EXECUTION_USE_STDEXEC
using ExecutionStopToken  = ex::inplace_stop_token;
using ExecutionStopSource = ex::inplace_stop_source;
#else
using ExecutionStopToken  = std::inplace_stop_token;
using ExecutionStopSource = std::inplace_stop_source;
#endif
struct ExecutionOutcome
{
    int    values  = 0;
    int    errors  = 0;
    int    stopped = 0;
    Result error{true};

    Function<void()> onValue;
    Function<void()> onStopped;
    Function<void()> onError;
    AsyncEventLoop*  loop = nullptr;
};
struct ExecutionEnvironment
{
    ExecutionStopToken token;

    auto query(ex::get_stop_token_t) const noexcept { return token; }
};
struct ExecutionReceiver
{
    using receiver_concept = ex::receiver_tag;

    ExecutionOutcome&  outcome;
    ExecutionStopToken token;

    auto get_env() const noexcept { return ExecutionEnvironment{token}; }
    template <typename... Values>
    void set_value(Values&&...) && noexcept
    {
        auto* loop = outcome.loop;
        ++outcome.values;
        if (outcome.onValue.isValid())
            outcome.onValue();
        if (loop)
            loop->interrupt();
    }
    void set_error(Result error) && noexcept
    {
        auto* loop = outcome.loop;
        ++outcome.errors;
        outcome.error = error;
        if (outcome.onError.isValid())
            outcome.onError();
        if (loop)
            loop->interrupt();
    }
    void set_stopped() && noexcept
    {
        auto* loop = outcome.loop;
        ++outcome.stopped;
        if (outcome.onStopped.isValid())
            outcome.onStopped();
        if (loop)
            loop->interrupt();
    }
};

struct AsyncExecutionTestCase : TestCase
{
    using TestCase::TestCase;
    template <typename Sender, typename... Outputs>
    Result consumeBounded(AsyncExecutionContext& context, Sender&& sender, Outputs&... outputs)
    {
        ExecutionStopSource stop;
        bool                expired = false;
        AsyncLoopTimeout    watchdog;
        watchdog.callback = [&](AsyncLoopTimeout::Result&)
        {
            expired = true;
            stop.request_stop();
        };
        SC_TRY(watchdog.start(context.getEventLoop(), {2000}));
        Result status{true};
        if constexpr (sizeof...(Outputs) == 0)
            status = context.run(forward<Sender>(sender), stop.get_token());
        else
            status = context.runIntoWithStop(forward<Sender>(sender), stop.get_token(), outputs...);
        if (not watchdog.isFree())
            SC_TRY(watchdog.unschedule(context.getEventLoop()));
        return expired ? Result(false) : status;
    }

    Result tcpPair(AsyncExecutionContext& context, SocketDescriptor& listener, SocketDescriptor& client,
                   AsyncExecutionConnection& accepted, uint16_t port, bool generalAddress = false)
    {
        SocketNetworking::initNetworking();
        SocketIPAddress address;
        SC_TRY(address.fromAddressPort("127.0.0.1", port));
        SC_TRY(context.getEventLoop().createAsyncTCPSocket(address.getAddressFamily(), listener));
        SC_TRY(context.getEventLoop().createAsyncTCPSocket(address.getAddressFamily(), client));
        SC_TRY(SocketServer(listener).bind(address));
        SC_TRY(SocketServer(listener).listen(2));
        auto connect =
            generalAddress ? context.connect(client, SocketAddress(address)) : context.connect(client, address);
        return consumeBounded(context, ex::when_all(context.accept(listener), move(connect)), accepted);
    }

    Result runBounded(AsyncEventLoop& loop)
    {
        AsyncLoopTimeout watchdog;
        bool             timedOut = false;
        watchdog.callback         = [&](AsyncLoopTimeout::Result&)
        {
            timedOut = true;
            loop.interrupt();
        };
        SC_TRY(watchdog.start(loop, {2000}));
        const Result status = loop.run();
        if (not watchdog.isFree())
            SC_TRY(watchdog.unschedule(loop));
        return Result(status and not timedOut);
    }
};
} // namespace
} // namespace SC
