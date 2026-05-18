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
#include "../../Libraries/Foundation/Deferred.h"
#include "../../Libraries/Process/Process.h"
#include "../../Libraries/Strings/Console.h"

namespace SC
{
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

    AwaitTask*     children[2] = {};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawn(success.task));
    SC_CO_TRY(group.spawn(failure.task));
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
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitProcessExitCodes event loop stopped before task completed");
    }
    SC_TRY(task.result());

    if (success.result.exitStatus != 0 or failure.result.exitStatus != 7)
    {
        return Result::Error("AwaitProcessExitCodes observed unexpected exit status");
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
        console.print("AwaitProcessExitCodes failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
