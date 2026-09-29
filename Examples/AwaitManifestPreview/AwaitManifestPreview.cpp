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
static constexpr ResultCategory AwaitManifestPreviewResultCategory = ResultCategory(0x8000001bu);
enum class AwaitManifestPreviewError : uint32_t
{
    PreviewCapacityExceeded = 1,
    CurrentDirectoryUnavailable,
};
static constexpr Result AwaitManifestPreviewFailure(AwaitManifestPreviewError error)
{
    return Result::Error(AwaitManifestPreviewResultCategory, error);
}
static ResultErrorFormat formatAwaitManifestPreviewError(Result result, Span<char> output)
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
    if (result.category() != AwaitManifestPreviewResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitManifestPreviewError>(result.errorValue()))
    {
    case AwaitManifestPreviewError::PreviewCapacityExceeded:
        formatter.append("Expected the whole manifest to fit in the preview buffer");
        break;
    case AwaitManifestPreviewError::CurrentDirectoryUnavailable:
        formatter.append("Could not resolve current working directory");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static AwaitTask readManifestPreview(AwaitEventLoop& await, ThreadPool& threadPool, FileDescriptor& file,
                                     Span<char> preview, AwaitFileReadResult& outResult)
{
    AwaitFileReadOptions options;
    options.threadPool = &threadPool;
    SC_CO_TRY(co_await await.fileReadUntilFullOrEOF(file, preview, outResult, options));

    if (not outResult.endOfFile)
    {
        co_return AwaitManifestPreviewFailure(AwaitManifestPreviewError::PreviewCapacityExceeded);
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
        return AwaitManifestPreviewFailure(AwaitManifestPreviewError::CurrentDirectoryUnavailable);
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
        return task.isCompleted() ? task.result() : runResult;
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
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitManifestPreviewError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitManifestPreview failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitManifestPreview failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
