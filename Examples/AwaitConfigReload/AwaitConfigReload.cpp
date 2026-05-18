// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that uses spawnAndWait() for a single child config load.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitConfigReload` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/Foundation/Deferred.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringView.h"
#include "../../Libraries/Threading/ThreadPool.h"

namespace SC
{
struct ConfigLoadJob
{
    StringSpan          path;
    AwaitTask           task;
    FileDescriptor      file;
    char                buffer[32] = {};
    AwaitFileReadResult result;
};

static AwaitTask readConfigFile(AwaitEventLoop& await, ThreadPool& threadPool, ConfigLoadJob& job)
{
    SC_CO_TRY(co_await await.fsOpen(threadPool, job.path, FileOpen::Read, job.file));
    SC_CO_TRY(co_await await.fsRead(threadPool, job.file, {job.buffer, sizeof(job.buffer)}, job.result));
    SC_CO_TRY(co_await await.fsClose(threadPool, job.file));

    co_return Result(true);
}

static AwaitTask reloadConfig(AwaitEventLoop& await, ThreadPool& threadPool, ConfigLoadJob& job)
{
    job.task = readConfigFile(await, threadPool, job);
    SC_CO_TRY(co_await await.spawnAndWait(job.task));

    if (job.result.data.sizeInBytes() == 0)
    {
        co_return Result::Error("AwaitConfigReload loaded an empty config");
    }

    co_return Result(true);
}

static Result runAwaitConfigReload()
{
    Console console;
    Console::tryAttachingToParentConsole();

    StringPath workingDirectory;
    StringSpan cwd = FileSystem::Operations::getCurrentWorkingDirectory(workingDirectory);
    if (cwd.isEmpty())
    {
        return Result::Error("AwaitConfigReload could not resolve current working directory");
    }

    StringPath path;
    SC_TRY(Path::join(path, {cwd, "await-config-reload.txt"}));

    FileSystem fs;
    SC_TRY(fs.writeString(path.view(), "mode=demo\n"));
    auto cleanup = MakeDeferred([&fs, &path] { (void)fs.removeFile(path.view()); });

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

    ConfigLoadJob job  = {path.view()};
    AwaitTask     task = reloadConfig(await, threadPool, job);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitConfigReload event loop stopped before task completed");
    }
    SC_TRY(task.result());

    StringView text({job.result.data.data(), job.result.data.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("AwaitConfigReload loaded config through spawnAndWait: {}", text);
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitConfigReload();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitConfigReload failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
