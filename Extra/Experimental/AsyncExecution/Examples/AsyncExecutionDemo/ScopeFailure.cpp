// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Extra/Experimental/AsyncExecution/AsyncExecution.h"
#include "Libraries/Time/Time.h"
#include <stdio.h>
#include <stdlib.h>

#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
namespace
{
using namespace SC;
namespace ex = AsyncExecutionProtocol;

// Fault injection for this example only: accept one live node, reject the next allocation.
struct FailSecondAllocation
{
    static constexpr size_t capacity = 32768;

    char storage[capacity + 255];

    unsigned attempts = 0;
    unsigned releases = 0;

    bool busy = false;

    AsyncExecutionScopeAllocator callbacks() noexcept
    {
        return {this,
                [](void* user, size_t size, size_t alignment) noexcept -> void*
                {
                    auto& self = *static_cast<FailSecondAllocation*>(user);
                    ++self.attempts;
                    if (self.attempts == 2 or self.busy or size > capacity or alignment > 256)
                        return nullptr;
                    self.busy              = true;
                    const size_t remainder = reinterpret_cast<size_t>(self.storage) % alignment;
                    return self.storage + (remainder == 0 ? 0 : alignment - remainder);
                },
                [](void* user, void*, size_t, size_t) noexcept
                {
                    auto& self = *static_cast<FailSecondAllocation*>(user);
                    self.busy  = false;
                    ++self.releases;
                }};
    }
};

Result runCase(bool childFails)
{
    AsyncExecution app;
    SC_TRY(app.create(1));
    auto& context = app.getContext();

    Atomic<bool> finished = false;
    Thread       guard;
    SC_TRY(guard.start(
        [&finished](Thread&)
        {
            const auto start = Time::Monotonic::now().getMonotonicMilliseconds();
            while (not finished.load() and Time::Monotonic::now().getMonotonicMilliseconds() - start < 4000)
                Thread::Sleep(1);
            if (not finished.load())
                ::_Exit(124);
        }));

    char                 borrowed[1]   = {'?'};
    bool                 childFinished = false;
    const Result         childError    = Result::Error(ResultCategory{0x80000023u}, 1);
    FailSecondAllocation allocation;
    AsyncExecutionScope  scope(context, allocation.callbacks());

    // Unstoppable deliberately models accepted work that must finish despite a shutdown request.
    auto first = ex::unstoppable(context.sleep({1}) | tryThen(
                                                          [&]() noexcept -> Result
                                                          {
                                                              borrowed[0]   = '!';
                                                              childFinished = true;
                                                              return childFails ? childError : Result(true);
                                                          }));

    // Application policy: never SC_TRY out of a used local scope before joining accepted work.
    Result     submission             = scope.spawn(move(first));
    const bool acceptedWorkWasPending = submission and not childFinished and allocation.busy;
    if (submission)
        submission = scope.spawn(context.sleep({0}));
    if (not submission)
        scope.requestStop();

    // Owner thread, outside callbacks, with no other active consumer. Keep both Results observable.
    const Result joined  = context.run(scope.join());
    const Result primary = submission ? joined : submission; // Submission error takes precedence here.
    const auto   summary = scope.getSummary();

    finished.store(true);
    SC_TRY(guard.join());
    printf("Submission=%u/%u; join=%u/%u; accepted=%zu succeeded=%zu failed=%zu stopped=%zu\n",
           submission.category().value, submission.errorValue(), joined.category().value, joined.errorValue(),
           summary.accepted, summary.succeeded, summary.failed, summary.stopped);

    // These are assertions about this deterministic example, after join has established quiescence.
    SC_TRY(primary.isError(AsyncExecutionResultCategory, AsyncExecutionError::OutOfMemory));
    SC_TRY(childFails ? joined.isError(childError.category(), childError.errorValue()) : bool(joined));
    SC_TRY(acceptedWorkWasPending and childFinished and borrowed[0] == '!' and summary.accepted == 1);
    SC_TRY(allocation.attempts == 2 and allocation.releases == 1 and not allocation.busy);
    borrowed[0] = '?';
    SC_TRY(context.getEventLoop().runNoWait());
    SC_TRY(borrowed[0] == '?');
    SC_TRY(app.close());
    return Result(true); // Both injected failure cases were handled and verified.
}
} // namespace
#endif

SC::Result runAsyncExecutionScopeFailureDemo()
{
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
    SC_TRY(runCase(false));
    return runCase(true);
#else
    fprintf(stderr, "Scope failure demo requires counting-scope support\n");
    return SC::Result(false);
#endif
}
