// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A small C++20 coroutine example where another thread wakes an Await task on the AsyncEventLoop.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitThreadWakeUp` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Common/Deferred.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"
#include "../../Libraries/Threading/Threading.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitThreadWakeUpResultCategory = ResultCategory(0x8000001fu);
enum class AwaitThreadWakeUpError : uint32_t
{
    WakeUpNotReceived = 1,
};
static constexpr Result AwaitThreadWakeUpFailure(AwaitThreadWakeUpError error)
{
    return Result::Error(AwaitThreadWakeUpResultCategory, error);
}
static ResultErrorFormat formatAwaitThreadWakeUpError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() != AwaitThreadWakeUpResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitThreadWakeUpError>(result.errorValue()))
    {
    case AwaitThreadWakeUpError::WakeUpNotReceived: formatter.append("Did not receive producer wake-up"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

struct WakeUpState
{
    AwaitLoopWakeUp       wakeUp;
    AwaitLoopWakeUpResult result;
};

static AwaitTask waitForProducer(AwaitEventLoop& await, WakeUpState& state)
{
    SC_CO_TRY(co_await await.wakeUp(state.wakeUp, state.result));
    if (state.result.deliveryCount == 0)
    {
        co_return AwaitThreadWakeUpFailure(AwaitThreadWakeUpError::WakeUpNotReceived);
    }
    co_return Result(true);
}

static Result runAwaitThreadWakeUp()
{
    Console console;
    Console::tryAttachingToParentConsole();

    AsyncEventLoop async;
    SC_TRY(async.create());
    auto closeAsync = MakeDeferred([&async] { (void)async.close(); });

    char           allocatorStorage[8 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    WakeUpState state;
    AwaitTask   task = waitForProducer(await, state);
    SC_TRY(await.spawn(task));

    struct ProducerContext
    {
        AwaitEventLoop*  await;
        AwaitLoopWakeUp* wakeUp;
        Result           result = Result(false);
    } producerContext = {&await, &state.wakeUp};

    Thread producer;
    SC_TRY(producer.start(
        [&producerContext](Thread& thread)
        {
            thread.setThreadName(SC_NATIVE_STR("await-producer"));
            producerContext.result = producerContext.wakeUp->wakeUp(*producerContext.await);
        }));
    SC_TRY(producer.join());
    SC_TRY(producerContext.result);

    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result() : runResult;
    }
    SC_TRY(task.result());

    console.print("AwaitThreadWakeUp delivery count: {}\n", state.result.deliveryCount);
    console.print("AwaitThreadWakeUp producer resumed coroutine on AsyncEventLoop\n");

    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitThreadWakeUp();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitThreadWakeUpError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitThreadWakeUp failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitThreadWakeUp failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
