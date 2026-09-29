// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A tiny C++20 coroutine example showing callback-style Async and Await sharing one event loop.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitCallbackBridge` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"
#include "../../Libraries/Time/Time.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitCallbackBridgeResultCategory = ResultCategory(0x80000012u);
enum class AwaitCallbackBridgeError : uint32_t
{
    CallbackNotInvoked = 1,
};
static constexpr Result AwaitCallbackBridgeFailure(AwaitCallbackBridgeError error)
{
    return Result::Error(AwaitCallbackBridgeResultCategory, error);
}
static ResultErrorFormat formatAwaitCallbackBridgeError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() != AwaitCallbackBridgeResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitCallbackBridgeError>(result.errorValue()))
    {
    case AwaitCallbackBridgeError::CallbackNotInvoked: formatter.append("Callback did not fire"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static AwaitTask waitForLegacyCallback(AwaitEventLoop& await, bool& callbackFired)
{
    SC_CO_TRY(co_await await.sleep(10_ms));
    if (not callbackFired)
    {
        co_return AwaitCallbackBridgeFailure(AwaitCallbackBridgeError::CallbackNotInvoked);
    }
    co_return Result(true);
}

static Result runAwaitCallbackBridge()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());

    char           allocatorStorage[8 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    bool             callbackFired = false;
    AsyncLoopTimeout legacyTimeout;
    legacyTimeout.callback = [&callbackFired](AsyncLoopTimeout::Result& result) { callbackFired = result.isValid(); };
    SC_TRY(legacyTimeout.start(async, 1_ms));

    AwaitTask task = waitForLegacyCallback(await, callbackFired);
    SC_TRY(await.spawn(task));

    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result() : runResult;
    }
    SC_TRY(task.result());

    console.print("AwaitCallbackBridge: legacy callback and coroutine shared one AsyncEventLoop\n");

    SC_TRY(async.close());
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitCallbackBridge();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitCallbackBridgeError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitCallbackBridge failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitCallbackBridge failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
