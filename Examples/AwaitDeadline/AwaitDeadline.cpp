// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that applies a deadline to a child task with AwaitEventLoop::waitFor().
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitDeadline` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Common/Deferred.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitDeadlineResultCategory = ResultCategory(0x80000015u);
enum class AwaitDeadlineError : uint32_t
{
    TimeoutCancellationMismatch = 1,
};
static constexpr Result AwaitDeadlineFailure(AwaitDeadlineError error)
{
    return Result::Error(AwaitDeadlineResultCategory, error);
}
static ResultErrorFormat formatAwaitDeadlineError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() != AwaitDeadlineResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitDeadlineError>(result.errorValue()))
    {
    case AwaitDeadlineError::TimeoutCancellationMismatch: formatter.append("Expected timeout cancellation"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static AwaitTask slowOperation(AwaitEventLoop& await)
{
    SC_CO_TRY(co_await await.sleep({1000}));
    co_return Result(true);
}

static AwaitTask deadlineWorkflow(AwaitEventLoop& await, AwaitTimeoutResult& timeout)
{
    AwaitTask child = slowOperation(await);
    SC_CO_TRY(await.spawn(child));

    Result waitResult = co_await await.waitFor(child, {1}, &timeout);
    if (waitResult or not timeout.timedOut or not child.isCompleted() or not AwaitIsCancelled(child.result()))
    {
        co_return AwaitDeadlineFailure(AwaitDeadlineError::TimeoutCancellationMismatch);
    }

    co_return Result(true);
}

static Result runAwaitDeadline()
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

    AwaitTimeoutResult timeout;
    AwaitTask          task = deadlineWorkflow(await, timeout);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result() : runResult;
    }
    SC_TRY(task.result());

    console.print("AwaitDeadline timed out: {}\n", timeout.timedOut ? 1 : 0);
    console.print("AwaitDeadline cancelled slow child without hidden allocation\n");

    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitDeadline();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitDeadlineError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitDeadline failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitDeadline failed: category {} error {}\n", result.category().value, result.errorValue());
        return -1;
    }
    return 0;
}
