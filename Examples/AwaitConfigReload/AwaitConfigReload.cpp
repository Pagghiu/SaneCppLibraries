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
#include "../../Libraries/Common/Deferred.h"
#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringView.h"
#include "../../Libraries/Threading/ThreadPool.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/File/FileErrorFormatter.h"
#include "../../Libraries/FileSystem/FileSystemErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitConfigReloadResultCategory = ResultCategory(0x80000013u);
enum class AwaitConfigReloadError : uint32_t
{
    EmptyConfiguration = 1,
    CurrentDirectoryUnavailable,
};
static constexpr Result AwaitConfigReloadFailure(AwaitConfigReloadError error)
{
    return Result::Error(AwaitConfigReloadResultCategory, error);
}
static ResultErrorFormat formatAwaitConfigReloadError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() == FileSystemResultCategory)
        return formatFileSystemError(result, output);
    if (result.category() == FileResultCategory)
        return formatFileError(result, output);
    if (result.category() != AwaitConfigReloadResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitConfigReloadError>(result.errorValue()))
    {
    case AwaitConfigReloadError::EmptyConfiguration: formatter.append("Loaded an empty config"); break;
    case AwaitConfigReloadError::CurrentDirectoryUnavailable:
        formatter.append("Could not resolve current working directory");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

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
        co_return AwaitConfigReloadFailure(AwaitConfigReloadError::EmptyConfiguration);
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
        return AwaitConfigReloadFailure(AwaitConfigReloadError::CurrentDirectoryUnavailable);
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
        return task.isCompleted() ? task.result() : runResult;
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
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitConfigReloadError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitConfigReload failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitConfigReload failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
