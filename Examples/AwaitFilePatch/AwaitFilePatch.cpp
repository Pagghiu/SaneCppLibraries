// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that patches a file in place with offset fileWrite().
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitFilePatch` from repo root.
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
static AwaitTask patchStatus(AwaitEventLoop& await, ThreadPool& threadPool, FileDescriptor& file, Span<char> readBuffer,
                             AwaitFileReadResult& readResult, AwaitFileWriteResult& writeResult)
{
    const char patch[] = {'P', 'A', 'I', 'D'};

    AwaitFileWriteOptions writeOptions;
    writeOptions.threadPool = &threadPool;
    writeOptions.useOffset  = true;
    writeOptions.offset     = 7;
    SC_CO_TRY(co_await await.fileWrite(file, {patch, sizeof(patch)}, &writeResult, writeOptions));

    AwaitFileReadOptions readOptions;
    readOptions.threadPool = &threadPool;
    readOptions.useOffset  = true;
    readOptions.offset     = 0;
    SC_CO_TRY(co_await await.fileRead(file, readBuffer, readResult, readOptions));

    co_return Result(true);
}

static Result runAwaitFilePatch()
{
    Console console;
    Console::tryAttachingToParentConsole();

    StringPath workingDirectory;
    StringSpan cwd = FileSystem::Operations::getCurrentWorkingDirectory(workingDirectory);
    if (cwd.isEmpty())
    {
        return Result::Error("AwaitFilePatch could not resolve current working directory");
    }

    StringPath path;
    SC_TRY(Path::join(path, {cwd, "await-file-patch.txt"}));

    FileSystem fs;
    SC_TRY(fs.writeString(path.view(), "status=____\n"));
    auto cleanup = MakeDeferred([&fs, &path] { (void)fs.removeFile(path.view()); });

    ThreadPool threadPool;
    SC_TRY(threadPool.create(2));
    auto destroyThreadPool = MakeDeferred([&threadPool] { (void)threadPool.destroy(); });

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    FileDescriptor file;
    SC_TRY(file.open(path.view(), FileOpen::ReadWrite));
    auto closeFile = MakeDeferred([&file] { (void)file.close(); });

    char           allocatorStorage[12 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    char                 readBuffer[12] = {};
    AwaitFileReadResult  readResult;
    AwaitFileWriteResult writeResult;
    AwaitTask task = patchStatus(await, threadPool, file, {readBuffer, sizeof(readBuffer)}, readResult, writeResult);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitFilePatch event loop stopped before task completed");
    }
    SC_TRY(task.result());

    if (writeResult.numBytes != 4)
    {
        return Result::Error("AwaitFilePatch wrote an unexpected number of bytes");
    }

    StringView text({readResult.data.data(), readResult.data.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("AwaitFilePatch patched invoice line: {}", text);
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitFilePatch();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitFilePatch failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
