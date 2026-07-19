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
    } state;

    constexpr size_t NumTasks = 3;

    FiberScheduler scheduler;
    FiberTask      tasks[NumTasks];
    char           stackMemory[NumTasks * 64 * 1024] = {};
    FiberTaskPool  pool(tasks, stackMemory, sizeof(stackMemory) / NumTasks);
    FiberTaskGroup group(scheduler);

    for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
    {
        auto task = [&state, taskIndex](FiberScheduler& scheduler)
        {
            for (int value = 0; value < 5; ++value)
            {
                state.partials[taskIndex] += static_cast<int>(taskIndex + 1) * value;
                SC_TRY(scheduler.yield());
            }
            return Result(true);
        };
        SC_TRY(group.spawn(pool, move(task)));
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
    } state;

    AsyncEventLoop eventLoop;
    SC_TRY(eventLoop.create());
    constexpr size_t NumTasks = 2;

    FiberScheduler scheduler;
    FiberTask      tasks[NumTasks];
    char           stackMemory[NumTasks * 64 * 1024] = {};
    FiberTaskPool  pool(tasks, stackMemory, 64 * 1024);
    FiberTaskGroup group(scheduler);

    FiberAsyncCommand commands[8];
    FiberAsyncIO      io(scheduler, eventLoop, commands);

    state.io  = &io;
    auto proc = [&state](FiberScheduler&)
    {
        SC_TRY(state.io->sleep(TimeMs{1}));
        state.completed++;
        return Result(true);
    };
    SC_TRY(group.spawn(pool, proc));
    SC_TRY(group.spawn(pool, proc));

    SC_TRY(io.runUntilComplete());

    size_t              numErrors = 0;
    FiberTaskGroupError errors[NumTasks];
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
    } state;

    AsyncEventLoop eventLoop;
    SC_TRY(eventLoop.create());

    FiberScheduler    scheduler;
    FiberTask         task;
    char              stackMemory[64 * 1024] = {};
    FiberStack        stack(stackMemory);
    FiberWorker       workers[NumWorkers];
    FiberWorkerThread threads[NumWorkers];
    FiberWorkerPool   workerPool;

    FiberAsyncCommand commands[8];
    FiberAsyncIO      io(scheduler, eventLoop, commands);

    state.io  = &io;
    auto proc = [&state](FiberScheduler&)
    {
        SC_TRY(state.io->sleep(TimeMs{1}));
        state.completed++;
        return Result(true);
    };

    SC_TRY(scheduler.spawn(task, stack, proc));

    SC_TRY(workerPool.start(scheduler, workers, threads));
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
