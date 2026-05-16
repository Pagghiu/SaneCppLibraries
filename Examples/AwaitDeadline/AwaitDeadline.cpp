// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that applies a deadline to a child task with AwaitEventLoop::waitFor().
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitDeadline` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Foundation/Deferred.h"
#include "../../Libraries/Strings/Console.h"

namespace SC
{
static AwaitTask slowOperation(AwaitEventLoop& await)
{
    SC_CO_TRY(co_await await.sleep({1000}));
    co_return Result(true);
}

static AwaitTask deadlineWorkflow(AwaitEventLoop& await, AwaitTimeoutResult& timeout)
{
    AwaitTask child = slowOperation(await);
    SC_CO_TRY(await.spawn(child));

    Result waitResult = co_await await.waitFor(child, {1}, &timeout);
    if (waitResult or not timeout.timedOut or not child.isCompleted() or not AwaitIsCancelled(child.result()))
    {
        co_return Result::Error("AwaitDeadline expected timeout cancellation");
    }

    co_return Result(true);
}

static Result runAwaitDeadline()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    char           arenaMemory[12 * 1024] = {};
    AwaitArena     arena({arenaMemory, sizeof(arenaMemory)});
    AwaitEventLoop await(async, &arena);

    AwaitTimeoutResult timeout;
    AwaitTask          task = deadlineWorkflow(await, timeout);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitDeadline event loop stopped before task completed");
    }
    SC_TRY(task.result());

    console.print("AwaitDeadline timed out: {}\n", timeout.timedOut ? 1 : 0);
    console.print("AwaitDeadline cancelled slow child without hidden allocation\n");

    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitDeadline();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitDeadline failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
