// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that runs CPU work on a ThreadPool with AwaitEventLoop::loopWork().
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitBackgroundDigest` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Foundation/Deferred.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Threading/ThreadPool.h"

namespace SC
{
struct DigestJob
{
    Span<const char> input;
    AwaitTask        task;
    uint32_t         digest = 0;
};

static uint32_t fnv1a(Span<const char> input)
{
    uint32_t digest = 2166136261u;
    for (size_t idx = 0; idx < input.sizeInBytes(); ++idx)
    {
        digest ^= static_cast<uint8_t>(input[idx]);
        digest *= 16777619u;
    }
    return digest;
}

static AwaitTask digestOne(AwaitEventLoop& await, ThreadPool& threadPool, DigestJob& job)
{
    Function<Result()> work = [&job]
    {
        job.digest = fnv1a(job.input);
        return Result(true);
    };
    SC_CO_TRY(co_await await.loopWork(threadPool, work));
    co_return Result(true);
}

static AwaitTask digestBoth(AwaitEventLoop& await, ThreadPool& threadPool, DigestJob& left, DigestJob& right)
{
    left.task  = digestOne(await, threadPool, left);
    right.task = digestOne(await, threadPool, right);

    AwaitTask*     children[2] = {};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawn(left.task));
    SC_CO_TRY(group.spawn(right.task));
    SC_CO_TRY(co_await group.waitAll());

    co_return Result(true);
}

static Result runAwaitBackgroundDigest()
{
    Console console;
    Console::tryAttachingToParentConsole();

    ThreadPool threadPool;
    SC_TRY(threadPool.create(2));
    auto destroyThreadPool = MakeDeferred([&threadPool] { (void)threadPool.destroy(); });

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    char           allocatorStorage[12 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    constexpr char leftInput[]  = "await background digest left";
    constexpr char rightInput[] = "await background digest right";
    DigestJob      left         = {{leftInput, sizeof(leftInput) - 1}};
    DigestJob      right        = {{rightInput, sizeof(rightInput) - 1}};
    AwaitTask      task         = digestBoth(await, threadPool, left, right);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitBackgroundDigest event loop stopped before task completed");
    }
    SC_TRY(task.result());

    console.print("AwaitBackgroundDigest left digest: {}\n", static_cast<uint64_t>(left.digest));
    console.print("AwaitBackgroundDigest right digest: {}\n", static_cast<uint64_t>(right.digest));
    console.print("AwaitBackgroundDigest kept work results in caller-owned jobs\n");

    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitBackgroundDigest();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitBackgroundDigest failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
