// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that races two caller-owned background jobs and keeps the first response.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitFirstResponse` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Strings/Console.h"

namespace SC
{
static AwaitTask queryMirror(AwaitEventLoop& await, int mirrorID, TimeMs latency, int& firstMirror)
{
    SC_CO_TRY(co_await await.sleep(latency));
    firstMirror = mirrorID;
    co_return Result(true);
}

static AwaitTask chooseFirstMirror(AwaitEventLoop& await, AwaitTaskRegistry& registry, int& firstMirror)
{
    SC_CO_TRY(registry.spawn(queryMirror(await, 1, {1}, firstMirror)));
    SC_CO_TRY(registry.spawn(queryMirror(await, 2, {1000}, firstMirror)));

    AwaitTaskRegistryWaitAnyResult waitAnyResult;
    SC_CO_TRY(co_await registry.waitAny(waitAnyResult));
    if (waitAnyResult.index != 0 or waitAnyResult.task == nullptr)
    {
        co_return Result::Error("AwaitFirstResponse expected mirror 1 to answer first");
    }

    co_return Result(true);
}

static Result runAwaitFirstResponse()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());

    char           arenaMemory[12 * 1024] = {};
    AwaitArena     arena({arenaMemory, sizeof(arenaMemory)});
    AwaitEventLoop await(async, &arena);

    AwaitTask         taskStorage[2];
    AwaitTaskRegistry registry(await, taskStorage);

    int       firstMirror = 0;
    AwaitTask coordinator = chooseFirstMirror(await, registry, firstMirror);
    SC_TRY(await.spawn(coordinator));
    SC_TRY(await.run());
    SC_TRY(coordinator.result());

    AwaitTaskGroupResultSummary summary;
    if (registry.clearCompleted(&summary) != 2 or summary.numSucceeded != 1 or summary.numFailed != 1)
    {
        return Result::Error("AwaitFirstResponse expected one winner and one cancelled mirror");
    }

    console.print("AwaitFirstResponse selected mirror {}\n", firstMirror);
    return async.close();
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitFirstResponse();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitFirstResponse failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
