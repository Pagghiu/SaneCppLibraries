// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A tiny C++20 coroutine example showing callback-style Async and Await sharing one event loop.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitCallbackBridge` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Time/Time.h"

namespace SC
{
static AwaitTask waitForLegacyCallback(AwaitEventLoop& await, bool& callbackFired)
{
    SC_CO_TRY(co_await await.sleep(10_ms));
    if (not callbackFired)
    {
        co_return Result::Error("AwaitCallbackBridge callback did not fire");
    }
    co_return Result(true);
}

static Result runAwaitCallbackBridge()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());

    char           arenaMemory[8 * 1024] = {};
    AwaitArena     arena({arenaMemory, sizeof(arenaMemory)});
    AwaitEventLoop await(async, &arena);

    bool             callbackFired = false;
    AsyncLoopTimeout legacyTimeout;
    legacyTimeout.callback = [&callbackFired](AsyncLoopTimeout::Result& result) { callbackFired = result.isValid(); };
    SC_TRY(legacyTimeout.start(async, 1_ms));

    AwaitTask task = waitForLegacyCallback(await, callbackFired);
    SC_TRY(await.spawn(task));

    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitCallbackBridge event loop stopped before task completed");
    }
    SC_TRY(task.result());

    console.print("AwaitCallbackBridge: legacy callback and coroutine shared one AsyncEventLoop\n");

    SC_TRY(async.close());
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitCallbackBridge();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitCallbackBridge failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
