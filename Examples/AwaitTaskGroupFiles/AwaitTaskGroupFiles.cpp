// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that reads two files concurrently with AwaitTaskGroup.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitTaskGroupFiles` console executable.
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
struct FileReadJob
{
    StringSpan          path;
    AwaitTask           task;
    FileDescriptor      file;
    char                buffer[64] = {};
    AwaitFileReadResult result;
};

static AwaitTask readOneFile(AwaitEventLoop& await, ThreadPool& threadPool, FileReadJob& job)
{
    SC_CO_TRY(co_await await.fsOpen(threadPool, job.path, FileOpen::Read, job.file));
    SC_CO_TRY(co_await await.fsRead(threadPool, job.file, {job.buffer, sizeof(job.buffer)}, job.result));
    SC_CO_TRY(co_await await.fsClose(threadPool, job.file));

    co_return Result(true);
}

static AwaitTask readBothFiles(AwaitEventLoop& await, ThreadPool& threadPool, FileReadJob& left, FileReadJob& right)
{
    left.task  = readOneFile(await, threadPool, left);
    right.task = readOneFile(await, threadPool, right);

    AwaitTask*     taskStorage[2] = {&left.task, &right.task};
    AwaitTaskGroup group(await, taskStorage);
    SC_CO_TRY(group.spawnAll(taskStorage));
    SC_CO_TRY(co_await group.waitAll());

    co_return Result(true);
}

static Result runAwaitTaskGroupFiles()
{
    Console console;
    Console::tryAttachingToParentConsole();

    StringPath workingDirectory;
    StringSpan cwd = FileSystem::Operations::getCurrentWorkingDirectory(workingDirectory);
    if (cwd.isEmpty())
    {
        return Result::Error("AwaitTaskGroupFiles could not resolve current working directory");
    }

    String leftPath;
    String rightPath;
    SC_TRY(Path::join(leftPath, {cwd, "await-left-note.txt"}));
    SC_TRY(Path::join(rightPath, {cwd, "await-right-note.txt"}));

    FileSystem fs;
    SC_TRY(fs.writeString(leftPath.view(), "left note from AwaitTaskGroup"));
    SC_TRY(fs.writeString(rightPath.view(), "right note from AwaitTaskGroup"));
    auto cleanup = MakeDeferred(
        [&fs, &leftPath, &rightPath]
        {
            (void)fs.removeFile(leftPath.view());
            (void)fs.removeFile(rightPath.view());
        });

    ThreadPool threadPool;
    SC_TRY(threadPool.create(2));
    auto destroyThreadPool = MakeDeferred([&threadPool] { (void)threadPool.destroy(); });

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    char           allocatorStorage[16 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    FileReadJob left  = {leftPath.view()};
    FileReadJob right = {rightPath.view()};
    AwaitTask   task  = readBothFiles(await, threadPool, left, right);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitTaskGroupFiles event loop stopped before task completed");
    }
    SC_TRY(task.result());

    StringView leftText({left.result.data.data(), left.result.data.sizeInBytes()}, false, StringEncoding::Ascii);
    StringView rightText({right.result.data.data(), right.result.data.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("Await group read left: {}\n", leftText);
    console.print("Await group read right: {}\n", rightText);
    console.print("Await group used caller storage for 2 tasks\n");

    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitTaskGroupFiles();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitTaskGroupFiles failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
