// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that runs detached background jobs in caller-owned registry slots.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitBackgroundJobs` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Strings/Console.h"

namespace SC
{
static AwaitTask warmCache(AwaitEventLoop& await, int& warmedEntries)
{
    SC_CO_TRY(co_await await.sleep({1}));
    warmedEntries = 3;
    co_return Result(true);
}

static AwaitTask flushMetrics(AwaitEventLoop& await, bool& metricsFlushed)
{
    SC_CO_TRY(co_await await.sleep({1}));
    metricsFlushed = true;
    co_return Result(true);
}

static Result runAwaitBackgroundJobs()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());

    char           allocatorStorage[12 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    AwaitTask         taskStorage[2];
    AwaitTaskRegistry registry(await, taskStorage);

    int  warmedEntries  = 0;
    bool metricsFlushed = false;
    SC_TRY(registry.spawn(warmCache(await, warmedEntries)));
    SC_TRY(registry.spawn(flushMetrics(await, metricsFlushed)));

    SC_TRY(await.run());

    AwaitTaskGroupResultSummary summary;
    const size_t                cleared = registry.clearCompleted(&summary);
    if (cleared != 2 or summary.numSucceeded != 2)
    {
        return Result::Error("AwaitBackgroundJobs expected two successful background jobs");
    }

    console.print("AwaitBackgroundJobs warmed {} entries\n", warmedEntries);
    console.print("AwaitBackgroundJobs metrics flushed: {}\n", metricsFlushed ? 1 : 0);
    return async.close();
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitBackgroundJobs();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitBackgroundJobs failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
