// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that waits for two child process exit codes concurrently.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitProcessExitCodes` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Common/Deferred.h"
#include "../../Libraries/Process/Process.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/Process/ProcessErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitProcessExitCodesResultCategory = ResultCategory(0x8000001cu);
enum class AwaitProcessExitCodesError : uint32_t
{
    ExitStatusMismatch = 1,
};
static constexpr Result AwaitProcessExitCodesFailure(AwaitProcessExitCodesError error)
{
    return Result::Error(AwaitProcessExitCodesResultCategory, error);
}
static ResultErrorFormat formatAwaitProcessExitCodesError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() == ProcessResultCategory)
        return formatProcessError(result, output);
    if (result.category() != AwaitProcessExitCodesResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitProcessExitCodesError>(result.errorValue()))
    {
    case AwaitProcessExitCodesError::ExitStatusMismatch: formatter.append("Observed unexpected exit status"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

struct ProcessWaitJob
{
    Process                process;
    AwaitTask              task;
    AwaitProcessExitResult result;
};

static AwaitTask waitOneProcess(AwaitEventLoop& await, ProcessWaitJob& job)
{
    SC_CO_TRY(co_await await.processExit(job.process.handle, job.result));
    co_return Result(true);
}

static AwaitTask waitBothProcesses(AwaitEventLoop& await, ProcessWaitJob& success, ProcessWaitJob& failure)
{
    success.task = waitOneProcess(await, success);
    failure.task = waitOneProcess(await, failure);

    AwaitTask*     children[2] = {&success.task, &failure.task};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawnAll(children));
    SC_CO_TRY(co_await group.waitAll());

    co_return Result(true);
}

static Result launchProcessExamples(ProcessWaitJob& success, ProcessWaitJob& failure)
{
#if SC_PLATFORM_WINDOWS
    SC_TRY(success.process.launch({"cmd", "/C", "exit 0"}));
    SC_TRY(failure.process.launch({"cmd", "/C", "exit 7"}));
#else
    SC_TRY(success.process.launch({"sh", "-c", "exit 0"}));
    SC_TRY(failure.process.launch({"sh", "-c", "exit 7"}));
#endif
    return Result(true);
}

static Result runAwaitProcessExitCodes()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    char           allocatorStorage[12 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    ProcessWaitJob success;
    ProcessWaitJob failure;
    SC_TRY(launchProcessExamples(success, failure));

    AwaitTask task = waitBothProcesses(await, success, failure);
    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result() : runResult;
    }
    SC_TRY(task.result());

    if (success.result.exitStatus != 0 or failure.result.exitStatus != 7)
    {
        return AwaitProcessExitCodesFailure(AwaitProcessExitCodesError::ExitStatusMismatch);
    }

    console.print("AwaitProcessExitCodes success status: {}\n", success.result.exitStatus);
    console.print("AwaitProcessExitCodes failure status: {}\n", failure.result.exitStatus);

    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitProcessExitCodes();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitProcessExitCodesError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitProcessExitCodes failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitProcessExitCodes failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
