// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A tiny Fibers + FibersAsync demo showing the same scheduler model for CPU work and async waits.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `FibersDemo` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Fibers/Fibers.h"
#include "../../Libraries/FibersAsync/FibersAsync.h"
#include "../../Libraries/Strings/Console.h"

namespace SC
{
static Result runCpuFibers(Console& console)
{
    //! [FibersCpuTasksSnippet]
    struct State
    {
        int partials[3] = {};
    };

    FiberScheduler scheduler;
    FiberTask      tasks[3];
    char           stackMemory[3 * 64 * 1024] = {};
    FiberTaskPool  pool({tasks, 3}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
    FiberTaskGroup group(scheduler);
    State          state;

    for (size_t taskIndex = 0; taskIndex < 3; ++taskIndex)
    {
        SC_TRY(group.spawn(pool, FiberTask::Procedure(
                                     [&state, taskIndex](FiberScheduler& scheduler)
                                     {
                                         for (int value = 0; value < 5; ++value)
                                         {
                                             state.partials[taskIndex] += static_cast<int>(taskIndex + 1) * value;
                                             SC_TRY(scheduler.yield());
                                         }
                                         return Result(true);
                                     })));
    }

    SC_TRY(group.waitAll());

    int total = 0;
    for (int partial : state.partials)
    {
        total += partial;
    }

    console.print("CPU fibers completed: partials=[{}, {}, {}], total={}\n", state.partials[0], state.partials[1],
                  state.partials[2], total);
    return Result(true);
    //! [FibersCpuTasksSnippet]
}

static Result runAsyncFibers(Console& console)
{
    //! [FibersAsyncSleepSnippet]
    struct State
    {
        FiberAsyncIO* io        = nullptr;
        int           completed = 0;
    };

    AsyncEventLoop eventLoop;
    SC_TRY(eventLoop.create());

    FiberScheduler      scheduler;
    FiberAsyncCommand   commands[8];
    FiberAsyncIO        io(scheduler, eventLoop, commands);
    FiberTask           tasks[2];
    char                stackMemory[2 * 64 * 1024] = {};
    FiberTaskPool       pool({tasks, 2}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
    FiberTaskGroup      group(scheduler);
    FiberTaskGroupError errors[2];
    State               state;
    state.io = &io;

    SC_TRY(group.spawn(pool, FiberTask::Procedure(
                                 [&state](FiberScheduler&)
                                 {
                                     SC_TRY(state.io->sleep(TimeMs{1}));
                                     state.completed++;
                                     return Result(true);
                                 })));
    SC_TRY(group.spawn(pool, FiberTask::Procedure(
                                 [&state](FiberScheduler&)
                                 {
                                     SC_TRY(state.io->sleep(TimeMs{1}));
                                     state.completed++;
                                     return Result(true);
                                 })));

    SC_TRY(io.runUntilComplete());

    size_t numErrors = 0;
    SC_TRY(group.collectErrors(errors, numErrors));
    if (numErrors != 0)
    {
        return errors[0].result;
    }

    console.print("FibersAsync sleeps completed: {}\n", state.completed);
    SC_TRY(eventLoop.close());
    return Result(true);
    //! [FibersAsyncSleepSnippet]
}

static Result runWorkerPoolAsyncFibers(Console& console)
{
    //! [FibersAsyncWorkerPoolSnippet]
    static constexpr size_t NumWorkers = 2;

    struct State
    {
        FiberAsyncIO* io        = nullptr;
        int           completed = 0;
    };

    AsyncEventLoop eventLoop;
    SC_TRY(eventLoop.create());

    FiberScheduler    scheduler;
    FiberAsyncCommand commands[8];
    FiberAsyncIO      io(scheduler, eventLoop, commands);
    FiberTask         task;
    char              stackMemory[64 * 1024] = {};
    FiberStack        stack({stackMemory, sizeof(stackMemory)});
    FiberWorker       workers[NumWorkers];
    FiberWorkerThread threads[NumWorkers];
    FiberWorkerPool   workerPool;
    State             state;
    state.io = &io;

    SC_TRY(scheduler.spawn(task, stack,
                           FiberTask::Procedure(
                               [&state](FiberScheduler&)
                               {
                                   SC_TRY(state.io->sleep(TimeMs{1}));
                                   state.completed++;
                                   return Result(true);
                               })));

    SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
    SC_TRY(io.runOwnerUntilComplete());
    SC_TRY(workerPool.join());
    SC_TRY(task.result());

    console.print("Worker-pool FibersAsync sleeps completed: {}\n", state.completed);
    SC_TRY(eventLoop.close());
    return Result(true);
    //! [FibersAsyncWorkerPoolSnippet]
}

static Result runFibersDemo()
{
    Console console;
    Console::tryAttachingToParentConsole();

    SC_TRY(runCpuFibers(console));
    SC_TRY(runAsyncFibers(console));
    SC_TRY(runWorkerPoolAsyncFibers(console));
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runFibersDemo();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("FibersDemo failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
