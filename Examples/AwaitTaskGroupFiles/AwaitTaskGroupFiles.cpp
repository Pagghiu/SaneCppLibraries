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
static constexpr ResultCategory AwaitTaskGroupFilesResultCategory = ResultCategory(0x8000001eu);
enum class AwaitTaskGroupFilesError : uint32_t
{
    CurrentDirectoryUnavailable = 1,
};
static constexpr Result AwaitTaskGroupFilesFailure(AwaitTaskGroupFilesError error)
{
    return Result::Error(AwaitTaskGroupFilesResultCategory, error);
}
static ResultErrorFormat formatAwaitTaskGroupFilesError(Result result, Span<char> output)
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
    if (result.category() != AwaitTaskGroupFilesResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitTaskGroupFilesError>(result.errorValue()))
    {
    case AwaitTaskGroupFilesError::CurrentDirectoryUnavailable:
        formatter.append("Could not resolve current working directory");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

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
        return AwaitTaskGroupFilesFailure(AwaitTaskGroupFilesError::CurrentDirectoryUnavailable);
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
        return task.isCompleted() ? task.result() : runResult;
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
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitTaskGroupFilesError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitTaskGroupFiles failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitTaskGroupFiles failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
