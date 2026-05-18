// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that reads a bounded manifest preview until EOF.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitManifestPreview` from repo root.
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
static AwaitTask readManifestPreview(AwaitEventLoop& await, ThreadPool& threadPool, FileDescriptor& file,
                                     Span<char> preview, AwaitFileReadResult& outResult)
{
    AwaitFileReadOptions options;
    options.threadPool = &threadPool;
    SC_CO_TRY(co_await await.fileReadUntilFullOrEOF(file, preview, outResult, options));

    if (not outResult.endOfFile)
    {
        co_return Result::Error("AwaitManifestPreview expected the whole manifest to fit in the preview buffer");
    }

    co_return Result(true);
}

static Result runAwaitManifestPreview()
{
    Console console;
    Console::tryAttachingToParentConsole();

    StringPath workingDirectory;
    StringSpan cwd = FileSystem::Operations::getCurrentWorkingDirectory(workingDirectory);
    if (cwd.isEmpty())
    {
        return Result::Error("AwaitManifestPreview could not resolve current working directory");
    }

    StringPath path;
    SC_TRY(Path::join(path, {cwd, "await-manifest-preview.txt"}));

    FileSystem fs;
    SC_TRY(fs.writeString(path.view(), "name=await-demo\nversion=1\n"));
    auto cleanup = MakeDeferred([&fs, &path] { (void)fs.removeFile(path.view()); });

    ThreadPool threadPool;
    SC_TRY(threadPool.create(2));
    auto destroyThreadPool = MakeDeferred([&threadPool] { (void)threadPool.destroy(); });

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    FileDescriptor file;
    SC_TRY(file.open(path.view(), FileOpen::Read));
    auto closeFile = MakeDeferred([&file] { (void)file.close(); });

    char           allocatorStorage[12 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    char                preview[64] = {};
    AwaitFileReadResult readResult;
    AwaitTask           task = readManifestPreview(await, threadPool, file, {preview, sizeof(preview)}, readResult);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitManifestPreview event loop stopped before task completed");
    }
    SC_TRY(task.result());

    StringView text({readResult.data.data(), readResult.data.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("AwaitManifestPreview read {} bytes:\n{}", readResult.data.sizeInBytes(), text);
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitManifestPreview();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitManifestPreview failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
