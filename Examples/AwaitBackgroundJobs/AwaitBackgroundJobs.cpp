// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that runs detached background jobs in caller-owned registry slots.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitBackgroundJobs` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitBackgroundJobsResultCategory = ResultCategory(0x80000010u);
enum class AwaitBackgroundJobsError : uint32_t
{
    JobCompletionMismatch = 1,
};
static constexpr Result AwaitBackgroundJobsFailure(AwaitBackgroundJobsError error)
{
    return Result::Error(AwaitBackgroundJobsResultCategory, error);
}
static ResultErrorFormat formatAwaitBackgroundJobsError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() != AwaitBackgroundJobsResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitBackgroundJobsError>(result.errorValue()))
    {
    case AwaitBackgroundJobsError::JobCompletionMismatch:
        formatter.append("Expected two successful background jobs");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static AwaitTask warmCache(AwaitEventLoop& await, int& warmedEntries)
{
    SC_CO_TRY(co_await await.sleep({1}));
    warmedEntries = 3;
    co_return Result(true);
}

static AwaitTask flushMetrics(AwaitEventLoop& await, bool& metricsFlushed)
{
    SC_CO_TRY(co_await await.sleep({1}));
    metricsFlushed = true;
    co_return Result(true);
}

static Result runAwaitBackgroundJobs()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());

    char           allocatorStorage[12 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    AwaitTask         taskStorage[2];
    AwaitTaskRegistry registry(await, taskStorage);

    int  warmedEntries  = 0;
    bool metricsFlushed = false;
    SC_TRY(registry.spawn(warmCache(await, warmedEntries)));
    SC_TRY(registry.spawn(flushMetrics(await, metricsFlushed)));

    SC_TRY(await.run());

    AwaitTaskGroupResultSummary summary;
    const size_t                cleared = registry.clearCompleted(&summary);
    if (cleared != 2 or summary.numSucceeded != 2)
    {
        return AwaitBackgroundJobsFailure(AwaitBackgroundJobsError::JobCompletionMismatch);
    }

    console.print("AwaitBackgroundJobs warmed {} entries\n", warmedEntries);
    console.print("AwaitBackgroundJobs metrics flushed: {}\n", metricsFlushed ? 1 : 0);
    return async.close();
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitBackgroundJobs();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitBackgroundJobsError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitBackgroundJobs failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitBackgroundJobs failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
