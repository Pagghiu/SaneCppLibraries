// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example where another thread wakes an Await task on the AsyncEventLoop.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitThreadWakeUp` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Foundation/Deferred.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Threading/Threading.h"

namespace SC
{
struct WakeUpState
{
    AwaitLoopWakeUp       wakeUp;
    AwaitLoopWakeUpResult result;
};

static AwaitTask waitForProducer(AwaitEventLoop& await, WakeUpState& state)
{
    SC_CO_TRY(co_await await.wakeUp(state.wakeUp, state.result));
    if (state.result.deliveryCount == 0)
    {
        co_return Result::Error("AwaitThreadWakeUp did not receive producer wake-up");
    }
    co_return Result(true);
}

static Result runAwaitThreadWakeUp()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    char           arenaMemory[8 * 1024] = {};
    AwaitArena     arena({arenaMemory, sizeof(arenaMemory)});
    AwaitEventLoop await(async, &arena);

    WakeUpState state;
    AwaitTask   task = waitForProducer(await, state);
    SC_TRY(await.spawn(task));

    struct ProducerContext
    {
        AwaitEventLoop*  await;
        AwaitLoopWakeUp* wakeUp;
        Result           result = Result(false);
    } producerContext = {&await, &state.wakeUp};

    Thread producer;
    SC_TRY(producer.start(
        [&producerContext](Thread& thread)
        {
            thread.setThreadName(SC_NATIVE_STR("await-producer"));
            producerContext.result = producerContext.wakeUp->wakeUp(*producerContext.await);
        }));
    SC_TRY(producer.join());
    SC_TRY(producerContext.result);

    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitThreadWakeUp event loop stopped before task completed");
    }
    SC_TRY(task.result());

    console.print("AwaitThreadWakeUp delivery count: {}\n", state.result.deliveryCount);
    console.print("AwaitThreadWakeUp producer resumed coroutine on AsyncEventLoop\n");

    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitThreadWakeUp();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitThreadWakeUp failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
