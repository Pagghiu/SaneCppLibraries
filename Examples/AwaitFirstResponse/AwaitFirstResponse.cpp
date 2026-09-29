// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example that races two caller-owned background jobs and keeps the first response.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitFirstResponse` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitFirstResponseResultCategory = ResultCategory(0x80000019u);
enum class AwaitFirstResponseError : uint32_t
{
    UnexpectedWinner = 1,
    CompletionMismatch,
};
static constexpr Result AwaitFirstResponseFailure(AwaitFirstResponseError error)
{
    return Result::Error(AwaitFirstResponseResultCategory, error);
}
static ResultErrorFormat formatAwaitFirstResponseError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() != AwaitFirstResponseResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitFirstResponseError>(result.errorValue()))
    {
    case AwaitFirstResponseError::UnexpectedWinner: formatter.append("Expected mirror 1 to answer first"); break;
    case AwaitFirstResponseError::CompletionMismatch:
        formatter.append("Expected one winner and one cancelled mirror");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static AwaitTask queryMirror(AwaitEventLoop& await, int mirrorID, TimeMs latency, int& firstMirror)
{
    SC_CO_TRY(co_await await.sleep(latency));
    firstMirror = mirrorID;
    co_return Result(true);
}

static AwaitTask chooseFirstMirror(AwaitEventLoop& await, AwaitTaskRegistry& registry, int& firstMirror)
{
    SC_CO_TRY(registry.spawn(queryMirror(await, 1, {1}, firstMirror)));
    SC_CO_TRY(registry.spawn(queryMirror(await, 2, {1000}, firstMirror)));

    AwaitTaskRegistryWaitAnyResult waitAnyResult;
    SC_CO_TRY(co_await registry.waitAny(waitAnyResult));
    if (waitAnyResult.index != 0 or waitAnyResult.task == nullptr)
    {
        co_return AwaitFirstResponseFailure(AwaitFirstResponseError::UnexpectedWinner);
    }

    co_return Result(true);
}

static Result runAwaitFirstResponse()
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

    int       firstMirror = 0;
    AwaitTask coordinator = chooseFirstMirror(await, registry, firstMirror);
    SC_TRY(await.spawn(coordinator));
    SC_TRY(await.run());
    SC_TRY(coordinator.result());

    AwaitTaskGroupResultSummary summary;
    if (registry.clearCompleted(&summary) != 2 or summary.numSucceeded != 1 or summary.numFailed != 1)
    {
        return AwaitFirstResponseFailure(AwaitFirstResponseError::CompletionMismatch);
    }

    console.print("AwaitFirstResponse selected mirror {}\n", firstMirror);
    return async.close();
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitFirstResponse();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitFirstResponseError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitFirstResponse failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitFirstResponse failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
