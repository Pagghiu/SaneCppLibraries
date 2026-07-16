// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Fibers/Fibers.h"
#include "Libraries/Fibers/Internal/FiberContext.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Atomic.h"
#include "Libraries/Threading/Threading.h"
#include "Libraries/Time/Time.h"

namespace SC
{
struct FibersTest;
}

struct SC::FibersTest : public SC::TestCase
{
    struct ContextState
    {
        FiberContext* main  = nullptr;
        FiberContext* fiber = nullptr;
        int           step  = 0;
    };

    FibersTest(SC::TestReport& report) : TestCase(report, "FibersTest")
    {
#if SC_COMPILER_FILC
        if (not report.quietMode)
        {
            report.console.printLine("FibersTest - Skipping under Fil-C: manual stack switching is unsupported");
        }
        return;
#else
        if (test_section("context switch"))
        {
            contextSwitch();
        }
        if (test_section("scheduler yield"))
        {
            schedulerYield();
        }
        if (test_section("explicit worker"))
        {
            explicitWorker();
        }
        if (test_section("cross thread resume"))
        {
            crossThreadResume();
        }
        if (test_section("run ready fibers"))
        {
            runReadyFibers();
        }
        if (test_section("concurrent scheduler run"))
        {
            concurrentSchedulerRun();
        }
        if (test_section("multi worker fairness"))
        {
            multiWorkerFairness();
        }
        if (test_section("cross thread wake and cancel"))
        {
            crossThreadWakeAndCancel();
        }
        if (test_section("cancellation resume race"))
        {
            cancellationResumeRace();
        }
        if (test_section("suspension publication races"))
        {
            suspensionPublicationRaces();
        }
        if (test_section("task state transition matrix"))
        {
            taskStateTransitionMatrix();
        }
        if (test_section("counter wait"))
        {
            counterWait();
        }
        if (test_section("task result"))
        {
            taskResult();
        }
        if (test_section("task user data"))
        {
            taskUserData();
        }
        if (test_section("fiber allocator"))
        {
            fiberAllocator();
        }
        if (test_section("task class"))
        {
            taskClass();
        }
        if (test_section("class availability wait"))
        {
            classAvailabilityWait();
        }
        if (test_section("class-backed task pool"))
        {
            classBackedTaskPool();
        }
        if (test_section("class-backed task pool cancellation"))
        {
            classBackedTaskPoolCancellation();
        }
        if (test_section("class-backed task pool workers"))
        {
            classBackedTaskPoolWorkers();
        }
        if (test_section("deadlock detection"))
        {
            deadlockDetection();
        }
        if (test_section("cooperative cancellation"))
        {
            cooperativeCancellation();
        }
        if (test_section("cancellation token"))
        {
            cancellationToken();
        }
        if (test_section("scheduler shutdown"))
        {
            schedulerShutdown();
        }
        if (test_section("cancel waiting task"))
        {
            cancelWaitingTask();
        }
        if (test_section("uninterruptible wait cancellation"))
        {
            uninterruptibleWaitCancellation();
        }
        if (test_section("task group"))
        {
            taskGroup();
        }
        if (test_section("task group errors"))
        {
            taskGroupErrors();
        }
        if (test_section("task pool"))
        {
            taskPool();
        }
        if (test_section("task pool availability wait"))
        {
            taskPoolAvailabilityWait();
        }
        if (test_section("task pool availability cancellation"))
        {
            taskPoolAvailabilityCancellation();
        }
        if (test_section("task pool availability worker pool"))
        {
            taskPoolAvailabilityWorkerPool();
        }
        if (test_section("task pool worker reuse pressure"))
        {
            taskPoolWorkerReusePressure();
        }
        if (test_section("task pool sustained stealing stress"))
        {
            taskPoolSustainedStealingStress();
        }
        if (test_section("task pool availability external producer"))
        {
            taskPoolAvailabilityExternalProducer();
        }
        if (test_section("worker pool"))
        {
            workerPool();
        }
        if (test_section("worker pool task waves"))
        {
            workerPoolTaskWaves();
        }
        if (test_section("worker pool primitives"))
        {
            workerPoolPrimitives();
        }
        if (test_section("worker pool counter cancellation stress"))
        {
            workerPoolCounterCancellationStress();
        }
        if (test_section("worker pool external spawn stress"))
        {
            workerPoolExternalSpawnStress();
        }
        if (test_section("worker pool concurrent external stress"))
        {
            workerPoolConcurrentExternalStress();
        }
        if (test_section("worker pool mixed transition stress"))
        {
            workerPoolMixedTransitionStress();
        }
        if (test_section("worker stealing"))
        {
            workerStealing();
        }
        if (test_section("worker deque"))
        {
            workerDeque();
        }
        if (test_section("worker deque external wake routing"))
        {
            workerDequeExternalWakeRouting();
        }
        if (test_section("worker pool deque"))
        {
            workerPoolDeque();
        }
        if (test_section("worker owner yield fast path"))
        {
            workerOwnerYieldFastPath();
        }
        if (test_section("worker completion fast path"))
        {
            workerCompletionFastPath();
        }
        if (test_section("counter completion fast path"))
        {
            counterCompletionFastPath();
        }
        if (test_section("worker counter completion fast path"))
        {
            workerCounterCompletionFastPath();
        }
        if (test_section("worker claim batch fast path"))
        {
            workerClaimBatchFastPath();
        }
        if (test_section("worker pool injection queue"))
        {
            workerPoolInjectionQueue();
        }
        if (test_section("scheduler diagnostics"))
        {
            schedulerDiagnostics();
        }
        if (test_section("trace hooks"))
        {
            traceHooks();
        }
        if (test_section("stack diagnostics"))
        {
            stackDiagnostics();
        }
        if (test_section("virtual stack"))
        {
            virtualStack();
        }
        if (test_section("stack class"))
        {
            stackClass();
        }
        if (test_section("fiber event"))
        {
            fiberEvent();
        }
        if (test_section("fiber auto-reset event"))
        {
            fiberAutoResetEvent();
        }
        if (test_section("fiber semaphore"))
        {
            fiberSemaphore();
        }
        if (test_section("fiber mutex"))
        {
            fiberMutex();
        }
        if (test_section("multi-worker primitives"))
        {
            multiWorkerPrimitives();
        }
        if (test_section("primitive cancellation"))
        {
            primitiveCancellation();
        }
        if (test_section("worker pool benchmark", TestCase::Execute::OnlyExplicit))
        {
            workerPoolBenchmark();
        }
#endif
    }

    static void contextEntry(void* userData)
    {
        ContextState& state = *static_cast<ContextState*>(userData);
        state.step          = 1;
        FiberContextOperations::switchTo(*state.fiber, *state.main);

        state.step = 3;
        FiberContextOperations::switchTo(*state.fiber, *state.main);
    }

    void contextSwitch()
    {
        FiberContext mainContext;
        FiberContext fiberContext;

        char         stackMemory[64 * 1024] = {};
        FiberStack   stack({stackMemory, sizeof(stackMemory)});
        ContextState state;
        state.main  = &mainContext;
        state.fiber = &fiberContext;

        SC_TEST_EXPECT(FiberContextOperations::captureCurrent(mainContext));
        SC_TEST_EXPECT(FiberContextOperations::create(fiberContext, stack.memory(), contextEntry, &state));

        FiberContextOperations::switchTo(mainContext, fiberContext);
        SC_TEST_EXPECT(state.step == 1);

        state.step = 2;
        FiberContextOperations::switchTo(mainContext, fiberContext);
        SC_TEST_EXPECT(state.step == 3);
    }

    void schedulerYield()
    {
        struct State
        {
            int steps[3] = {};
            int index    = 0;
        };

        FiberScheduler scheduler;
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        State state;
        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               state.steps[state.index++] = 1;
                                               SC_TRY(scheduler.yield());
                                               state.steps[state.index++] = 2;
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.hasReadyFibers());
        SC_TEST_EXPECT(scheduler.readyFiberCount() == 1);
        SC_TEST_EXPECT(scheduler.hasActiveFibers());
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(not task.isCompleted());
        SC_TEST_EXPECT(state.index == 1);
        SC_TEST_EXPECT(state.steps[0] == 1);
        SC_TEST_EXPECT(scheduler.readyFiberCount() == 1);

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(scheduler.readyFiberCount() == 0);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(state.index == 2);
        SC_TEST_EXPECT(state.steps[1] == 2);
    }

    void explicitWorker()
    {
        struct State
        {
            FiberTask*   task           = nullptr;
            FiberWorker* worker         = nullptr;
            bool         sawTaskRunning = false;
        };

        FiberScheduler scheduler;
        FiberWorker    worker;
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.task   = &task;
        state.worker = &worker;

        SC_TEST_EXPECT(not worker.isActive());
        SC_TEST_EXPECT(worker.scheduler() == nullptr);
        SC_TEST_EXPECT(worker.runningTask() == nullptr);

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               state.sawTaskRunning = scheduler.currentTask() == state.task and
                                                                      state.worker->runningTask() == state.task and
                                                                      state.worker->scheduler() == &scheduler;
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.run(worker));
        SC_TEST_EXPECT(state.sawTaskRunning);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not worker.isActive());
        SC_TEST_EXPECT(worker.scheduler() == nullptr);
        SC_TEST_EXPECT(worker.runningTask() == nullptr);
    }

    void crossThreadResume()
    {
        struct State
        {
            FiberScheduler* scheduler      = nullptr;
            uint64_t        firstThreadID  = 0;
            uint64_t        secondThreadID = 0;
            Result          runResult      = Result(true);
        };

        FiberScheduler scheduler;
        FiberTask      task;
        static char    stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.scheduler = &scheduler;
        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               state.firstThreadID = Thread::CurrentThreadID();
                                               SC_TRY(scheduler.yield());
                                               state.secondThreadID = Thread::CurrentThreadID();
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Ready);

        Thread workerThread;
        auto   worker = [&state](Thread&) { state.runResult = state.scheduler->run(); };
        SC_TEST_EXPECT(workerThread.start(worker));
        SC_TEST_EXPECT(workerThread.join());

        SC_TEST_EXPECT(state.runResult);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(state.firstThreadID != 0);
        SC_TEST_EXPECT(state.secondThreadID != 0);
        SC_TEST_EXPECT(state.firstThreadID != state.secondThreadID);
    }

    void runReadyFibers()
    {
        struct State
        {
            FiberCounter* counter = nullptr;
            int           step    = 0;
        };

        FiberScheduler scheduler;
        FiberCounter   counter;
        FiberTask      waitingTask;
        FiberTask      completingTask;

        char       waitingStackMemory[64 * 1024]    = {};
        char       completingStackMemory[64 * 1024] = {};
        FiberStack waitingStack({waitingStackMemory, sizeof(waitingStackMemory)});
        FiberStack completingStack({completingStackMemory, sizeof(completingStackMemory)});

        State state;
        state.counter = &counter;
        scheduler.add(counter);

        SC_TEST_EXPECT(scheduler.spawn(waitingTask, waitingStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               state.step = 1;
                                               SC_TRY(scheduler.wait(*state.counter));
                                               state.step = 3;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(completingTask, completingStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.step = 2;
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.runReadyFibers());
        SC_TEST_EXPECT(state.step == 2);
        SC_TEST_EXPECT(waitingTask.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(completingTask.isCompleted());
        SC_TEST_EXPECT(not scheduler.hasReadyFibers());
        SC_TEST_EXPECT(scheduler.readyFiberCount() == 0);
        SC_TEST_EXPECT(scheduler.hasActiveFibers());

        SC_TEST_EXPECT(scheduler.done(counter));
        SC_TEST_EXPECT(scheduler.readyFiberCount() == 1);
        SC_TEST_EXPECT(scheduler.runReadyFibers());
        SC_TEST_EXPECT(state.step == 3);
        SC_TEST_EXPECT(waitingTask.isCompleted());
        SC_TEST_EXPECT(waitingTask.result());
        SC_TEST_EXPECT(scheduler.readyFiberCount() == 0);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
    }

    void concurrentSchedulerRun()
    {
        static constexpr size_t NumTasks   = 8;
        static constexpr size_t NumWorkers = 4;
        static constexpr int    NumYields  = 16;

        struct State
        {
            FiberScheduler* scheduler = nullptr;
            int             steps[NumTasks];
            bool            workerSucceeded[NumWorkers];
        };

        FiberScheduler scheduler;
        FiberTask      tasks[NumTasks];
        static char    stackMemory[NumTasks][64 * 1024] = {};

        FiberStack stacks[NumTasks] = {
            FiberStack({stackMemory[0], sizeof(stackMemory[0])}), FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
            FiberStack({stackMemory[2], sizeof(stackMemory[2])}), FiberStack({stackMemory[3], sizeof(stackMemory[3])}),
            FiberStack({stackMemory[4], sizeof(stackMemory[4])}), FiberStack({stackMemory[5], sizeof(stackMemory[5])}),
            FiberStack({stackMemory[6], sizeof(stackMemory[6])}), FiberStack({stackMemory[7], sizeof(stackMemory[7])}),
        };

        State state;
        state.scheduler = &scheduler;
        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            state.steps[idx] = 0;
        }
        for (size_t idx = 0; idx < NumWorkers; ++idx)
        {
            state.workerSucceeded[idx] = true;
        }

        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                           FiberTask::Procedure(
                                               [&state, idx](FiberScheduler& scheduler)
                                               {
                                                   for (int step = 0; step < NumYields; ++step)
                                                   {
                                                       state.steps[idx] += 1;
                                                       SC_TRY(scheduler.yield());
                                                   }
                                                   state.steps[idx] += 1;
                                                   return Result(true);
                                               })));
        }

        Thread workers[NumWorkers];
        for (size_t idx = 0; idx < NumWorkers; ++idx)
        {
            SC_TEST_EXPECT(workers[idx].start(
                [&state, idx](Thread&)
                {
                    while (state.scheduler->hasActiveFibers())
                    {
                        Result result = state.scheduler->runNoWait();
                        if (not result)
                        {
                            state.workerSucceeded[idx] = false;
                            return;
                        }
                    }
                }));
        }
        for (size_t idx = 0; idx < NumWorkers; ++idx)
        {
            SC_TEST_EXPECT(workers[idx].join());
            SC_TEST_EXPECT(state.workerSucceeded[idx]);
        }

        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(tasks[idx].isCompleted());
            SC_TEST_EXPECT(tasks[idx].result());
            SC_TEST_EXPECT(state.steps[idx] == NumYields + 1);
        }
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
    }

    void multiWorkerFairness()
    {
        static constexpr size_t NumTasks   = 8;
        static constexpr size_t NumWorkers = 3;
        static constexpr int    NumYields  = 16;

        struct State
        {
            FiberScheduler* scheduler = nullptr;
            Atomic<int32_t> steps[NumTasks];
            Atomic<bool>    workerSucceeded[NumWorkers];
        };

        FiberScheduler scheduler;
        FiberTask      tasks[NumTasks];
        static char    stackMemory[NumTasks][64 * 1024] = {};

        FiberStack stacks[NumTasks] = {
            FiberStack({stackMemory[0], sizeof(stackMemory[0])}), FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
            FiberStack({stackMemory[2], sizeof(stackMemory[2])}), FiberStack({stackMemory[3], sizeof(stackMemory[3])}),
            FiberStack({stackMemory[4], sizeof(stackMemory[4])}), FiberStack({stackMemory[5], sizeof(stackMemory[5])}),
            FiberStack({stackMemory[6], sizeof(stackMemory[6])}), FiberStack({stackMemory[7], sizeof(stackMemory[7])}),
        };

        State state;
        state.scheduler = &scheduler;
        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            state.steps[idx].store(0);
        }
        for (size_t idx = 0; idx < NumWorkers; ++idx)
        {
            state.workerSucceeded[idx].store(true);
        }

        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                           FiberTask::Procedure(
                                               [&state, idx](FiberScheduler& scheduler)
                                               {
                                                   for (int step = 0; step < NumYields; ++step)
                                                   {
                                                       state.steps[idx].fetch_add(1);
                                                       SC_TRY(scheduler.yield());
                                                   }
                                                   state.steps[idx].fetch_add(1);
                                                   return Result(true);
                                               })));
        }

        Thread workers[NumWorkers];
        for (size_t idx = 0; idx < NumWorkers; ++idx)
        {
            SC_TEST_EXPECT(workers[idx].start(
                [&state, idx](Thread&)
                {
                    while (state.scheduler->hasActiveFibers())
                    {
                        Result result = state.scheduler->runNoWait();
                        if (not result)
                        {
                            state.workerSucceeded[idx].store(false);
                            return;
                        }
                    }
                }));
        }
        for (size_t idx = 0; idx < NumWorkers; ++idx)
        {
            SC_TEST_EXPECT(workers[idx].join());
            SC_TEST_EXPECT(state.workerSucceeded[idx].load());
        }

        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(tasks[idx].isCompleted());
            SC_TEST_EXPECT(tasks[idx].result());
            SC_TEST_EXPECT(state.steps[idx].load() == NumYields + 1);
        }
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
    }

    void crossThreadWakeAndCancel()
    {
        struct State
        {
            FiberScheduler* scheduler       = nullptr;
            FiberTask*      task            = nullptr;
            FiberCounter*   counter         = nullptr;
            bool            readyCanceled   = false;
            bool            waitingCanceled = false;
            bool            counterWoke     = false;
            bool            threadSucceeded = true;
        };

        {
            FiberScheduler scheduler;
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.scheduler = &scheduler;
            state.task      = &task;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   Result yieldResult  = scheduler.yield();
                                                   state.readyCanceled = not yieldResult;
                                                   return yieldResult;
                                               })));

            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(task.status() == FiberTaskStatus::Ready);

            Thread cancelThread;
            SC_TEST_EXPECT(cancelThread.start(
                [&state](Thread&)
                {
                    Result result = state.scheduler->requestCancel(*state.task);
                    if (not result)
                    {
                        state.threadSucceeded = false;
                    }
                }));
            SC_TEST_EXPECT(cancelThread.join());

            SC_TEST_EXPECT(state.threadSucceeded);
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(state.readyCanceled);
        }

        {
            FiberScheduler scheduler;
            FiberCounter   counter;
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.scheduler = &scheduler;
            state.task      = &task;
            state.counter   = &counter;

            scheduler.add(counter);
            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   Result waitResult     = scheduler.wait(*state.counter);
                                                   state.waitingCanceled = not waitResult;
                                                   return waitResult;
                                               })));

            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(task.status() == FiberTaskStatus::Waiting);

            Thread cancelThread;
            SC_TEST_EXPECT(cancelThread.start(
                [&state](Thread&)
                {
                    Result result = state.scheduler->requestCancel(*state.task);
                    if (not result)
                    {
                        state.threadSucceeded = false;
                    }
                }));
            SC_TEST_EXPECT(cancelThread.join());

            SC_TEST_EXPECT(state.threadSucceeded);
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(state.waitingCanceled);
        }

        {
            FiberScheduler scheduler;
            FiberCounter   counter;
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.scheduler = &scheduler;
            state.counter   = &counter;

            scheduler.add(counter);
            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(scheduler.wait(*state.counter));
                                                   state.counterWoke = true;
                                                   return Result(true);
                                               })));

            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(task.status() == FiberTaskStatus::Waiting);

            Thread doneThread;
            SC_TEST_EXPECT(doneThread.start(
                [&state](Thread&)
                {
                    Result result = state.scheduler->done(*state.counter);
                    if (not result)
                    {
                        state.threadSucceeded = false;
                    }
                }));
            SC_TEST_EXPECT(doneThread.join());

            SC_TEST_EXPECT(state.threadSucceeded);
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(state.counterWoke);
        }
    }

    void cancellationResumeRace()
    {
        static constexpr size_t NumTasks   = 32;
        static constexpr size_t NumWaiters = NumTasks / 2;
        static constexpr size_t NumWorkers = 4;

        struct State
        {
            Atomic<int32_t> entered;
            Atomic<int32_t> cancelled;
            Atomic<bool>    startRace;
            Atomic<bool>    allowExit;
        };
        struct Context
        {
            State*        state        = nullptr;
            FiberCounter* counter      = nullptr;
            FiberCounter* startCounter = nullptr;
        };

        FiberScheduler    scheduler;
        static FiberTask  tasks[NumTasks];
        static char       stackMemory[NumTasks * 64 * 1024] = {};
        FiberTaskPool     taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
        FiberCounter      counters[NumWaiters];
        FiberCounter      startCounter;
        Context           contexts[NumTasks];
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread workerThreads[NumWorkers];
        FiberWorkerPool   workerPool;
        State             state;

        for (FiberCounter& counter : counters)
        {
            scheduler.add(counter);
        }
        scheduler.add(startCounter);
        for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
        {
            contexts[taskIndex].state = &state;
            if (taskIndex < NumWaiters)
            {
                contexts[taskIndex].counter = &counters[taskIndex];
            }
            else
            {
                contexts[taskIndex].startCounter = &startCounter;
            }
            Context* context = &contexts[taskIndex];
            SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                         [context](FiberScheduler& scheduler)
                                                         {
                                                             context->state->entered.fetch_add(1);
                                                             if (context->counter != nullptr)
                                                             {
                                                                 Result waitResult = scheduler.wait(*context->counter);
                                                                 if (not waitResult)
                                                                 {
                                                                     context->state->cancelled.fetch_add(1);
                                                                     return waitResult;
                                                                 }
                                                             }
                                                             else
                                                             {
                                                                 Result waitResult =
                                                                     scheduler.wait(*context->startCounter);
                                                                 if (not waitResult)
                                                                 {
                                                                     context->state->cancelled.fetch_add(1);
                                                                     return waitResult;
                                                                 }
                                                             }
                                                             while (not context->state->allowExit.load())
                                                             {
                                                                 Result yieldResult = scheduler.yield();
                                                                 if (not yieldResult)
                                                                 {
                                                                     context->state->cancelled.fetch_add(1);
                                                                     return yieldResult;
                                                                 }
                                                             }
                                                             Result yieldResult = scheduler.yield();
                                                             if (not yieldResult)
                                                             {
                                                                 context->state->cancelled.fetch_add(1);
                                                             }
                                                             return yieldResult;
                                                         })));
        }

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {workerThreads, NumWorkers}));
        bool allTasksWaiting = false;
        for (size_t attempt = 0; attempt < 5000; ++attempt)
        {
            allTasksWaiting = state.entered.load() == static_cast<int32_t>(NumTasks);
            for (FiberTask& task : tasks)
            {
                allTasksWaiting = allTasksWaiting and task.status() == FiberTaskStatus::Waiting;
            }
            if (allTasksWaiting)
            {
                break;
            }
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(allTasksWaiting);
        if (not allTasksWaiting)
        {
            for (FiberTask& task : tasks)
            {
                SC_TEST_EXPECT(scheduler.requestCancel(task));
            }
            for (FiberCounter& counter : counters)
            {
                SC_TEST_EXPECT(scheduler.done(counter));
            }
            SC_TEST_EXPECT(scheduler.done(startCounter));
            state.allowExit.store(true);
            SC_TEST_EXPECT(workerPool.join());
            return;
        }

        struct RaceThreadState
        {
            FiberScheduler* scheduler = nullptr;
            FiberTask*      tasks     = nullptr;
            FiberCounter*   counters  = nullptr;
            State*          state     = nullptr;
            Atomic<bool>    succeeded = true;
        } raceState;
        raceState.scheduler = &scheduler;
        raceState.tasks     = tasks;
        raceState.counters  = counters;
        raceState.state     = &state;

        Thread wakeThread;
        Thread cancelThread;
        SC_TEST_EXPECT(wakeThread.start(
            [&raceState](Thread&)
            {
                while (not raceState.state->startRace.load())
                {
                    Thread::Sleep(1);
                }
                for (size_t counterIndex = 0; counterIndex < NumWaiters; ++counterIndex)
                {
                    if (not raceState.scheduler->done(raceState.counters[counterIndex]))
                    {
                        raceState.succeeded.store(false);
                    }
                }
            }));
        SC_TEST_EXPECT(cancelThread.start(
            [&raceState](Thread&)
            {
                while (not raceState.state->startRace.load())
                {
                    Thread::Sleep(1);
                }
                for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
                {
                    if (not raceState.scheduler->requestCancel(raceState.tasks[taskIndex]))
                    {
                        raceState.succeeded.store(false);
                    }
                }
                raceState.state->allowExit.store(true);
            }));
        SC_TEST_EXPECT(scheduler.done(startCounter));
        state.startRace.store(true);
        SC_TEST_EXPECT(wakeThread.join());
        SC_TEST_EXPECT(cancelThread.join());
        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(raceState.succeeded.load());
        SC_TEST_EXPECT(state.cancelled.load() == static_cast<int32_t>(NumTasks));
        SC_TEST_EXPECT(taskPool.availableCount() == NumTasks);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        for (FiberTask& task : tasks)
        {
            SC_TEST_EXPECT(not task.result());
        }
    }

    void suspensionPublicationRaces()
    {
        struct State
        {
            FiberScheduler* scheduler       = nullptr;
            FiberCounter*   counter         = nullptr;
            FiberTask*      task            = nullptr;
            Result          callbackResult  = Result(true);
            bool            cancel          = false;
            bool            callbackInvoked = false;
            bool            observedRunning = false;
            bool            resumed         = false;
        };

        for (size_t raceIndex = 0; raceIndex < 2; ++raceIndex)
        {
            const bool     cancel = raceIndex != 0;
            FiberScheduler scheduler;
            FiberCounter   counter;
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});
            State          state;

            state.scheduler = &scheduler;
            state.counter   = &counter;
            state.task      = &task;
            state.cancel    = cancel;

            FiberTraceHooks hooks;
            hooks.userData = &state;
            hooks.callback = [](void* userData, const FiberTraceEvent& event)
            {
                State& state = *static_cast<State*>(userData);
                if (event.type != FiberTraceEventType::TaskWaiting)
                {
                    return;
                }
                state.callbackInvoked = true;
                state.observedRunning = state.task->status() == FiberTaskStatus::Running;
                state.callbackResult =
                    state.cancel ? state.scheduler->requestCancel(*state.task) : state.scheduler->done(*state.counter);
            };
            scheduler.setTraceHooks(hooks);
            scheduler.add(counter);
            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state, &counter](FiberScheduler& scheduler)
                                               {
                                                   Result waitResult = scheduler.wait(counter);
                                                   state.resumed     = true;
                                                   return waitResult;
                                               })));
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(state.callbackInvoked);
            SC_TEST_EXPECT(state.observedRunning);
            SC_TEST_EXPECT(state.callbackResult);
            SC_TEST_EXPECT(state.resumed);
            SC_TEST_EXPECT(cancel ? not task.result() : task.result());
            SC_TEST_EXPECT(task.status() == FiberTaskStatus::Completed);
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            if (cancel)
            {
                SC_TEST_EXPECT(scheduler.done(counter));
            }
        }
    }

    void taskStateTransitionMatrix()
    {
        struct State
        {
            FiberTask* task = nullptr;

            size_t started    = 0;
            size_t yielded    = 0;
            size_t waiting    = 0;
            size_t completed  = 0;
            bool   validState = true;
        };

        FiberScheduler scheduler;
        FiberCounter   counter;
        FiberTask      task;
        char           stackMemory[FiberStackSize::SixtyFourKiB] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});
        State          state;
        state.task = &task;

        FiberTraceHooks hooks;
        hooks.userData = &state;
        hooks.callback = [](void* userData, const FiberTraceEvent& event)
        {
            State& state = *static_cast<State*>(userData);
            switch (event.type)
            {
            case FiberTraceEventType::TaskStarted:
                state.started += 1;
                state.validState = state.validState and state.task->status() == FiberTaskStatus::Running;
                break;
            case FiberTraceEventType::TaskYielded:
                state.yielded += 1;
                state.validState = state.validState and state.task->status() == FiberTaskStatus::Running;
                break;
            case FiberTraceEventType::TaskWaiting:
                state.waiting += 1;
                state.validState = state.validState and state.task->status() == FiberTaskStatus::Running;
                break;
            case FiberTraceEventType::TaskCompleted:
                state.completed += 1;
                state.validState = state.validState and state.task->status() == FiberTaskStatus::Completing;
                break;
            }
        };
        scheduler.setTraceHooks(hooks);
        scheduler.add(counter);

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&counter](FiberScheduler& currentScheduler)
                                           {
                                               SC_TRY(currentScheduler.yield());
                                               SC_TRY(currentScheduler.wait(counter));
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Ready);

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Ready);

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Waiting);

        SC_TEST_EXPECT(scheduler.done(counter));
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Ready);

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Completed);
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(state.validState);
        SC_TEST_EXPECT(state.started == 3);
        SC_TEST_EXPECT(state.yielded == 1);
        SC_TEST_EXPECT(state.waiting == 1);
        SC_TEST_EXPECT(state.completed == 1);
    }

    void counterWait()
    {
        struct State
        {
            FiberTask*  childTask  = nullptr;
            FiberStack* childStack = nullptr;
            int         step       = 0;
        };

        FiberScheduler scheduler;
        FiberCounter   rootCounter;
        FiberTask      parentTask;
        FiberTask      childTask;

        char       parentStackMemory[64 * 1024] = {};
        char       childStackMemory[64 * 1024]  = {};
        FiberStack parentStack({parentStackMemory, sizeof(parentStackMemory)});
        FiberStack childStack({childStackMemory, sizeof(childStackMemory)});

        State state;
        state.childTask  = &childTask;
        state.childStack = &childStack;

        SC_TEST_EXPECT(scheduler.spawn(parentTask, parentStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               FiberCounter childCounter;
                                               SC_TRY(scheduler.spawn(*state.childTask, *state.childStack,
                                                                      FiberTask::Procedure(
                                                                          [&state](FiberScheduler&)
                                                                          {
                                                                              state.step = 2;
                                                                              return Result(true);
                                                                          }),
                                                                      &childCounter));
                                               state.step = 1;
                                               SC_TRY(scheduler.wait(childCounter));
                                               SC_TRY_MSG(state.step == 2,
                                                          "Child fiber did not complete before parent resumed");
                                               state.step = 3;
                                               return Result(true);
                                           }),
                                       &rootCounter));

        SC_TEST_EXPECT(rootCounter.value() == 1);
        SC_TEST_EXPECT(scheduler.wait(rootCounter));
        SC_TEST_EXPECT(rootCounter.value() == 0);
        SC_TEST_EXPECT(state.step == 3);
        SC_TEST_EXPECT(parentTask.isCompleted());
        SC_TEST_EXPECT(childTask.isCompleted());
        SC_TEST_EXPECT(parentTask.result());
        SC_TEST_EXPECT(childTask.result());
    }

    void taskResult()
    {
        FiberScheduler scheduler;
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        SC_TEST_EXPECT(scheduler.spawn(
            task, stack,
            FiberTask::Procedure([](FiberScheduler&) { return Result::Error("Expected fiber failure"); })));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
    }

    void taskUserData()
    {
        struct State
        {
            FiberTask* task       = nullptr;
            bool       sawCurrent = false;
            bool       sawPooled  = false;
            int        value      = 0;
        };

        FiberScheduler scheduler;
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.task = &task;
        task.setUserData(&state);
        SC_TEST_EXPECT(task.userData() == &state);

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [](FiberScheduler& scheduler)
                                           {
                                               FiberTask* current = scheduler.currentTask();
                                               if (current == nullptr)
                                               {
                                                   return Result::Error("Expected current fiber task");
                                               }
                                               State* state = static_cast<State*>(current->userData());
                                               if (state == nullptr)
                                               {
                                                   return Result::Error("Expected task user data");
                                               }
                                               state->sawCurrent = current == state->task;
                                               state->value += 1;
                                               SC_TRY(scheduler.yield());
                                               state->value += 1;
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(state.sawCurrent);
        SC_TEST_EXPECT(state.value == 2);
        SC_TEST_EXPECT(task.userData() == &state);

        task.setUserData(nullptr);
        SC_TEST_EXPECT(task.userData() == nullptr);

        FiberTask     poolTasks[1];
        char          poolStackMemory[64 * 1024] = {};
        FiberTaskPool pool({poolTasks, 1}, {poolStackMemory, sizeof(poolStackMemory)}, 64 * 1024);
        FiberTask*    spawnedTask = nullptr;

        FiberTaskSpawnOptions options;
        options.userData    = &state;
        options.setUserData = true;
        SC_TEST_EXPECT(pool.spawn(scheduler,
                                  FiberTask::Procedure(
                                      [](FiberScheduler& scheduler)
                                      {
                                          FiberTask* current = scheduler.currentTask();
                                          if (current == nullptr)
                                          {
                                              return Result::Error("Expected current fiber task");
                                          }
                                          State* state = static_cast<State*>(current->userData());
                                          if (state == nullptr)
                                          {
                                              return Result::Error("Expected task user data");
                                          }
                                          state->sawPooled = current == state->task;
                                          state->value += 1;
                                          return Result(true);
                                      }),
                                  options, &spawnedTask));
        state.task = spawnedTask;
        SC_TEST_EXPECT(spawnedTask != nullptr);
        if (spawnedTask != nullptr)
        {
            SC_TEST_EXPECT(spawnedTask->userData() == &state);
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(spawnedTask->isCompleted());
            SC_TEST_EXPECT(spawnedTask->result());
        }
        SC_TEST_EXPECT(state.sawPooled);
        SC_TEST_EXPECT(state.value == 3);
    }

    struct TrackingFiberAllocatorInterface : public FiberAllocatorInterface
    {
        void*  lastOwner      = nullptr;
        size_t lastNumBytes   = 0;
        size_t lastAlignment  = 0;
        size_t numAllocations = 0;
        size_t numReleases    = 0;
        size_t used           = 0;
        char   storage[1024]  = {};

        virtual void* allocateImpl(const void* owner, size_t numBytes, size_t alignment) override
        {
            const size_t address        = reinterpret_cast<size_t>(storage + used);
            const size_t alignedAddress = (address + alignment - 1) & ~(alignment - 1);
            const size_t offset         = alignedAddress - reinterpret_cast<size_t>(storage);
            if (offset + numBytes > sizeof(storage))
            {
                return nullptr;
            }

            lastOwner     = const_cast<void*>(owner);
            lastNumBytes  = numBytes;
            lastAlignment = alignment;
            numAllocations++;
            used = offset + numBytes;
            return storage + offset;
        }

        virtual void releaseImpl(void*) override { numReleases++; }
    };

    void fiberAllocator()
    {
        char fixedMemory[2048] = {};

        FiberAllocator fixed;
        SC_TEST_EXPECT(fixed.createFixed(fixedMemory));
        SC_TEST_EXPECT(fixed.mode() == FiberAllocatorMode::Fixed);
        SC_TEST_EXPECT(fixed.capacity() == sizeof(fixedMemory));
        void* fixedA = fixed.allocate(nullptr, 64, alignof(void*));
        void* fixedB = fixed.allocate(nullptr, 32, alignof(void*));
        SC_TEST_EXPECT(fixedA != nullptr);
        SC_TEST_EXPECT(fixedB != nullptr);
        SC_TEST_EXPECT(fixed.used() > 0);
        SC_TEST_EXPECT(fixed.peakUsed() >= fixed.used());
        FiberAllocator::releaseFromAnyAllocator(fixedA);
        fixed.release(fixedB);
        SC_TEST_EXPECT(fixed.used() == 0);
        SC_TEST_EXPECT(fixed.statistics().numAllocations == 2);
        SC_TEST_EXPECT(fixed.statistics().numReleases == 2);
        SC_TEST_EXPECT(fixed.statistics().requestedBytesAllocated == 96);
        SC_TEST_EXPECT(fixed.statistics().requestedBytesReleased == 96);
        SC_TEST_EXPECT(fixed.close());

        char           smallMemory[1] = {};
        FiberAllocator small;
        SC_TEST_EXPECT(small.createFixed(smallMemory));
        SC_TEST_EXPECT(small.allocate(nullptr, 16, alignof(void*)) == nullptr);
        SC_TEST_EXPECT(small.statistics().numAllocationFailures == 1);
        SC_TEST_EXPECT(small.failedAllocationSize() == 16);
        SC_TEST_EXPECT(small.close());

        FiberAllocator mallocAllocator;
        SC_TEST_EXPECT(mallocAllocator.createMalloc());
        void* mallocMemory = mallocAllocator.allocate(nullptr, 128, alignof(void*));
        SC_TEST_EXPECT(mallocMemory != nullptr);
        SC_TEST_EXPECT(mallocAllocator.statistics().requestedBytesAllocated == 128);
        mallocAllocator.release(mallocMemory);
        SC_TEST_EXPECT(mallocAllocator.statistics().requestedBytesReleased == 128);
        SC_TEST_EXPECT(mallocAllocator.used() == 0);
        SC_TEST_EXPECT(mallocAllocator.close());

        TrackingFiberAllocatorInterface tracking;
        FiberAllocator                  polymorphic;
        int                             owner = 0;
        SC_TEST_EXPECT(polymorphic.createPolymorphic(tracking));
        void* polymorphicMemory = polymorphic.allocate(&owner, 24, alignof(void*));
        SC_TEST_EXPECT(polymorphicMemory != nullptr);
        SC_TEST_EXPECT(tracking.lastOwner == &owner);
        SC_TEST_EXPECT(tracking.lastNumBytes >= 24);
        SC_TEST_EXPECT(tracking.lastAlignment >= alignof(void*));
        polymorphic.release(polymorphicMemory);
        SC_TEST_EXPECT(tracking.numAllocations == 1);
        SC_TEST_EXPECT(tracking.numReleases == 1);
        SC_TEST_EXPECT(polymorphic.close());

        FiberAllocator virtualAllocator;
        SC_TEST_EXPECT(not virtualAllocator.createVirtual({}));
        SC_TEST_EXPECT(virtualAllocator.createVirtual({64 * 1024, 0}));
        SC_TEST_EXPECT(virtualAllocator.mode() == FiberAllocatorMode::Virtual);
        SC_TEST_EXPECT(virtualAllocator.reservedBytes() >= 64 * 1024);
        SC_TEST_EXPECT(virtualAllocator.committedBytes() == 0);
        void* virtualMemory = virtualAllocator.allocate(nullptr, 1024, alignof(void*));
        SC_TEST_EXPECT(virtualMemory != nullptr);
        SC_TEST_EXPECT(virtualAllocator.committedBytes() > 0);
        virtualAllocator.release(virtualMemory);
        SC_TEST_EXPECT(virtualAllocator.used() == 0);
        SC_TEST_EXPECT(virtualAllocator.close());

        char           liveMemoryStorage[1024] = {};
        FiberAllocator liveAllocator;
        SC_TEST_EXPECT(liveAllocator.createFixed(liveMemoryStorage));
        void* liveMemory = liveAllocator.allocate(nullptr, 64, alignof(void*));
        SC_TEST_EXPECT(liveMemory != nullptr);
        SC_TEST_EXPECT(not liveAllocator.validateClose());
        liveAllocator.release(liveMemory);
        SC_TEST_EXPECT(liveAllocator.validateClose());
        SC_TEST_EXPECT(liveAllocator.close());
    }

    void taskClass()
    {
        FiberTaskClass taskClass;
        FiberTask*     task = nullptr;

        FiberAllocator closedAllocator;
        SC_TEST_EXPECT(not taskClass.create(closedAllocator, {3}));

        char           memory[64 * 1024] = {};
        FiberAllocator allocator;
        SC_TEST_EXPECT(allocator.createFixed(memory));
        SC_TEST_EXPECT(not taskClass.create(allocator, {}));
        SC_TEST_EXPECT(taskClass.create(allocator, {3}));
        SC_TEST_EXPECT(taskClass.isOpen());
        SC_TEST_EXPECT(taskClass.capacity() == 3);
        SC_TEST_EXPECT(taskClass.activeCount() == 0);
        SC_TEST_EXPECT(taskClass.availableCount() == 3);

        FiberTask* tasks[3] = {};
        SC_TEST_EXPECT(taskClass.acquire(tasks[0]));
        SC_TEST_EXPECT(taskClass.acquire(tasks[1]));
        SC_TEST_EXPECT(taskClass.acquire(tasks[2]));
        SC_TEST_EXPECT(tasks[0] != tasks[1]);
        SC_TEST_EXPECT(tasks[1] != tasks[2]);
        SC_TEST_EXPECT(taskClass.owns(*tasks[0]));
        SC_TEST_EXPECT(not taskClass.acquire(task));
        SC_TEST_EXPECT(task == nullptr);
        SC_TEST_EXPECT(not taskClass.validateClose());
        SC_TEST_EXPECT(not taskClass.close());

        FiberTaskClassDiagnostics diagnostics;
        taskClass.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.capacity == 3);
        SC_TEST_EXPECT(diagnostics.activeTasks == 3);
        SC_TEST_EXPECT(diagnostics.availableTasks == 0);
        SC_TEST_EXPECT(diagnostics.peakActiveTasks == 3);

        SC_TEST_EXPECT(taskClass.release(*tasks[1]));
        SC_TEST_EXPECT(not taskClass.release(*tasks[1]));
        SC_TEST_EXPECT(taskClass.acquire(task));
        SC_TEST_EXPECT(task == tasks[1]);

        FiberTask foreignTask;
        SC_TEST_EXPECT(not taskClass.release(foreignTask));

        FiberScheduler scheduler;
        char           stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});
        bool           ran = false;
        SC_TEST_EXPECT(scheduler.spawn(*tasks[0], stack,
                                       FiberTask::Procedure(
                                           [&ran](FiberScheduler&)
                                           {
                                               ran = true;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(not taskClass.release(*tasks[0]));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(ran);
        SC_TEST_EXPECT(taskClass.release(*tasks[0]));
        SC_TEST_EXPECT(taskClass.release(*tasks[2]));
        SC_TEST_EXPECT(taskClass.release(*task));
        SC_TEST_EXPECT(taskClass.validateClose());
        SC_TEST_EXPECT(taskClass.close());
        SC_TEST_EXPECT(not taskClass.isOpen());
        SC_TEST_EXPECT(allocator.used() == 0);
        SC_TEST_EXPECT(allocator.close());
    }

    void classAvailabilityWait()
    {
        char           taskMemory[64 * 1024] = {};
        FiberAllocator allocator;
        FiberTaskClass taskClass;
        SC_TEST_EXPECT(allocator.createFixed(taskMemory));
        SC_TEST_EXPECT(taskClass.create(allocator, {1}));

        FiberTask* heldTask = nullptr;
        SC_TEST_EXPECT(taskClass.acquire(heldTask));

        FiberScheduler scheduler;
        FiberTask      waiter;
        char           waiterStackMemory[64 * 1024] = {};
        FiberStack     waiterStack({waiterStackMemory, sizeof(waiterStackMemory)});
        bool           taskWaitResumed = false;
        SC_TEST_EXPECT(scheduler.spawn(waiter, waiterStack,
                                       FiberTask::Procedure(
                                           [&taskClass, &taskWaitResumed](FiberScheduler& currentScheduler)
                                           {
                                               SC_TRY(taskClass.waitForAvailableSlot(currentScheduler));
                                               taskWaitResumed = true;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(waiter.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(taskClass.release(*heldTask));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(taskWaitResumed);

        SC_TEST_EXPECT(taskClass.acquire(heldTask));
        bool taskWaitCancelled = false;
        SC_TEST_EXPECT(scheduler.spawn(waiter, waiterStack,
                                       FiberTask::Procedure(
                                           [&taskClass, &taskWaitCancelled](FiberScheduler& currentScheduler)
                                           {
                                               taskWaitCancelled = not taskClass.waitForAvailableSlot(currentScheduler);
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(scheduler.requestCancel(waiter));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(taskWaitCancelled);
        SC_TEST_EXPECT(taskClass.release(*heldTask));
        SC_TEST_EXPECT(taskClass.close());
        SC_TEST_EXPECT(allocator.close());

        FiberStackClass stackClass;
        FiberStack      heldStack({});
        SC_TEST_EXPECT(stackClass.reserve({64 * 1024, 1, false}));
        SC_TEST_EXPECT(stackClass.acquire(heldStack));

        bool stackWaitResumed = false;
        SC_TEST_EXPECT(scheduler.spawn(waiter, waiterStack,
                                       FiberTask::Procedure(
                                           [&stackClass, &stackWaitResumed](FiberScheduler& currentScheduler)
                                           {
                                               SC_TRY(stackClass.waitForAvailableSlot(currentScheduler));
                                               stackWaitResumed = true;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(waiter.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(stackClass.release(heldStack));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(stackWaitResumed);

        SC_TEST_EXPECT(stackClass.acquire(heldStack));
        bool stackWaitCancelled = false;
        SC_TEST_EXPECT(scheduler.spawn(waiter, waiterStack,
                                       FiberTask::Procedure(
                                           [&stackClass, &stackWaitCancelled](FiberScheduler& currentScheduler)
                                           {
                                               stackWaitCancelled =
                                                   not stackClass.waitForAvailableSlot(currentScheduler);
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(scheduler.requestCancel(waiter));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(stackWaitCancelled);
        SC_TEST_EXPECT(stackClass.release(heldStack));
        stackClass.release();
    }

    void classBackedTaskPool()
    {
        struct State
        {
            FiberTaskPool* pool           = nullptr;
            int            completed      = 0;
            bool           producerWaited = false;
        };

        char            taskMemory[128 * 1024] = {};
        FiberAllocator  allocator;
        FiberTaskClass  taskClass;
        FiberStackClass stackClass;
        FiberTaskPool   pool;
        SC_TEST_EXPECT(allocator.createFixed(taskMemory));
        SC_TEST_EXPECT(taskClass.create(allocator, {2}));
        SC_TEST_EXPECT(stackClass.reserve({64 * 1024, 2, false}));
        SC_TEST_EXPECT(pool.create(taskClass, stackClass));
        SC_TEST_EXPECT(pool.capacity() == 2);
        SC_TEST_EXPECT(pool.availableCount() == 2);
        SC_TEST_EXPECT(pool.stackSizeInBytes() >= 64 * 1024);

        FiberTaskPoolDiagnostics diagnostics;
        pool.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.classBacked);
        SC_TEST_EXPECT(diagnostics.capacity == 2);
        SC_TEST_EXPECT(diagnostics.taskClass.capacity == 2);
        SC_TEST_EXPECT(diagnostics.taskClass.availableTasks == 2);
        SC_TEST_EXPECT(diagnostics.stackClass.capacity == 2);
        SC_TEST_EXPECT(diagnostics.stackClass.activeStacks == 0);

        FiberTaskPool secondPool;
        SC_TEST_EXPECT(not secondPool.create(taskClass, stackClass));

        FiberScheduler scheduler;
        FiberTask      producer;
        char           producerStackMemory[64 * 1024] = {};
        FiberStack     producerStack({producerStackMemory, sizeof(producerStackMemory)});
        State          state;
        state.pool = &pool;
        SC_TEST_EXPECT(scheduler.spawn(producer, producerStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& currentScheduler)
                                           {
                                               state.producerWaited = state.pool->availableCount() == 0;
                                               SC_TRY(state.pool->waitForSpawnCapacity(currentScheduler));
                                               return state.pool->spawn(currentScheduler, FiberTask::Procedure(
                                                                                              [&state](FiberScheduler&)
                                                                                              {
                                                                                                  state.completed += 1;
                                                                                                  return Result(true);
                                                                                              }));
                                           })));

        FiberTask* firstTask  = nullptr;
        FiberTask* secondTask = nullptr;
        SC_TEST_EXPECT(pool.spawn(scheduler,
                                  FiberTask::Procedure(
                                      [&state](FiberScheduler&)
                                      {
                                          state.completed += 1;
                                          return Result(true);
                                      }),
                                  &firstTask));
        SC_TEST_EXPECT(pool.spawn(scheduler,
                                  FiberTask::Procedure(
                                      [&state](FiberScheduler&)
                                      {
                                          state.completed += 1;
                                          return Result(true);
                                      }),
                                  &secondTask));
        SC_TEST_EXPECT(firstTask != nullptr);
        SC_TEST_EXPECT(secondTask != nullptr);
        SC_TEST_EXPECT(firstTask != secondTask);
        SC_TEST_EXPECT(pool.availableCount() == 0);
        pool.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.activeTasks == 2);
        SC_TEST_EXPECT(diagnostics.availableTasks == 0);
        SC_TEST_EXPECT(diagnostics.taskClass.activeTasks == 2);
        SC_TEST_EXPECT(diagnostics.stackClass.activeStacks == 2);
        SC_TEST_EXPECT(not pool.close());
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.producerWaited);
        SC_TEST_EXPECT(state.completed == 3);
        SC_TEST_EXPECT(pool.activeCount() == 0);
        SC_TEST_EXPECT(pool.availableCount() == 2);
        pool.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.taskClass.peakActiveTasks == 2);
        SC_TEST_EXPECT(diagnostics.stackClass.peakActiveStacks == 2);
        SC_TEST_EXPECT(pool.close());
        SC_TEST_EXPECT(taskClass.close());
        stackClass.release();
        SC_TEST_EXPECT(allocator.close());
    }

    void classBackedTaskPoolCancellation()
    {
        struct State
        {
            FiberTaskPool* pool          = nullptr;
            bool           waitCancelled = false;
        };

        char            taskMemory[64 * 1024] = {};
        FiberAllocator  allocator;
        FiberTaskClass  taskClass;
        FiberStackClass stackClass;
        FiberTaskPool   pool;
        SC_TEST_EXPECT(allocator.createFixed(taskMemory));
        SC_TEST_EXPECT(taskClass.create(allocator, {1}));
        SC_TEST_EXPECT(stackClass.reserve({64 * 1024, 1, false}));
        SC_TEST_EXPECT(pool.create(taskClass, stackClass));

        FiberScheduler scheduler;
        FiberTask*     pooledTask = nullptr;
        SC_TEST_EXPECT(pool.spawn(scheduler,
                                  FiberTask::Procedure(
                                      [](FiberScheduler& currentScheduler)
                                      {
                                          for (;;)
                                          {
                                              SC_TRY(currentScheduler.yield());
                                          }
                                      }),
                                  &pooledTask));

        FiberTask  producer;
        char       producerStackMemory[64 * 1024] = {};
        FiberStack producerStack({producerStackMemory, sizeof(producerStackMemory)});
        State      state;
        state.pool = &pool;
        SC_TEST_EXPECT(scheduler.spawn(producer, producerStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& currentScheduler)
                                           {
                                               state.waitCancelled =
                                                   not state.pool->waitForSpawnCapacity(currentScheduler);
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(producer.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(scheduler.requestCancel(producer));
        SC_TEST_EXPECT(scheduler.requestCancel(*pooledTask));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.waitCancelled);
        SC_TEST_EXPECT(producer.result());
        SC_TEST_EXPECT(not pooledTask->result());
        SC_TEST_EXPECT(pool.availableCount() == 1);
        SC_TEST_EXPECT(pool.close());
        SC_TEST_EXPECT(taskClass.close());
        stackClass.release();
        SC_TEST_EXPECT(allocator.close());
    }

    void classBackedTaskPoolWorkers()
    {
        static constexpr size_t NumWorkers = 4;
        static constexpr size_t NumSlots   = 256;
        static constexpr size_t NumJobs    = 256;

        struct State
        {
            FiberTaskPool*  pool = nullptr;
            FiberEvent*     gate = nullptr;
            Atomic<int32_t> submitted;
            Atomic<int32_t> completed;
        };

        static char     taskMemory[NumSlots * sizeof(FiberTask) + NumSlots + 4096] = {};
        FiberAllocator  allocator;
        FiberTaskClass  taskClass;
        FiberStackClass stackClass;
        FiberTaskPool   pool;
        SC_TEST_EXPECT(allocator.createFixed(taskMemory));
        SC_TEST_EXPECT(taskClass.create(allocator, {NumSlots}));
        SC_TEST_EXPECT(stackClass.reserve({32 * 1024, NumSlots, true}));
        SC_TEST_EXPECT(pool.create(taskClass, stackClass));

        FiberScheduler scheduler;
        FiberTask      producer;
        static char    producerStackMemory[64 * 1024] = {};
        FiberStack     producerStack({producerStackMemory, sizeof(producerStackMemory)});
        FiberEvent     gate;
        State          state;
        state.pool = &pool;
        state.gate = &gate;
        SC_TEST_EXPECT(scheduler.spawn(
            producer, producerStack,
            FiberTask::Procedure(
                [&state](FiberScheduler& currentScheduler)
                {
                    for (size_t idx = 0; idx < NumJobs; ++idx)
                    {
                        while (not state.pool->hasAvailableTask())
                        {
                            SC_TRY(state.pool->waitForSpawnCapacity(currentScheduler));
                        }
                        SC_TRY(state.pool->spawn(currentScheduler, FiberTask::Procedure(
                                                                       [&state](FiberScheduler& childScheduler)
                                                                       {
                                                                           SC_TRY(state.gate->wait(childScheduler));
                                                                           for (size_t yield = 0; yield < 4; ++yield)
                                                                           {
                                                                               SC_TRY(childScheduler.yield());
                                                                           }
                                                                           state.completed.fetch_add(1);
                                                                           return Result(true);
                                                                       })));
                        state.submitted.fetch_add(1);
                        if (idx + 1 == NumSlots)
                        {
                            SC_TRY(state.gate->signal(currentScheduler));
                        }
                    }
                    return Result(true);
                })));

        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;
        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(producer.result());
        SC_TEST_EXPECT(state.submitted.load() == static_cast<int32_t>(NumJobs));
        SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumJobs));
        SC_TEST_EXPECT(pool.activeCount() == 0);
        SC_TEST_EXPECT(pool.availableCount() == NumSlots);

        FiberTaskPoolDiagnostics diagnostics;
        pool.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.taskClass.peakActiveTasks == NumSlots);
        SC_TEST_EXPECT(diagnostics.stackClass.peakActiveStacks == NumSlots);
        SC_TEST_EXPECT(diagnostics.stackClass.committedSizeBytes < diagnostics.stackClass.peakCommittedBytes);
        SC_TEST_EXPECT(pool.close());
        SC_TEST_EXPECT(taskClass.close());
        stackClass.release();
        SC_TEST_EXPECT(allocator.close());
    }

    void deadlockDetection()
    {
        FiberScheduler scheduler;
        FiberCounter   counter;
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        scheduler.add(counter);
        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&counter](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.wait(counter));
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Waiting);

        Result deadlock = scheduler.runOnce();
        SC_TEST_EXPECT(not deadlock);

        SC_TEST_EXPECT(scheduler.done(counter));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
    }

    void cooperativeCancellation()
    {
        struct State
        {
            FiberTask* task        = nullptr;
            bool       sawCurrent  = false;
            bool       sawCancel   = false;
            int        yieldPoints = 0;
        };

        FiberScheduler scheduler;
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.task = &task;

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               state.sawCurrent = scheduler.currentTask() == state.task;
                                               for (;;)
                                               {
                                                   state.yieldPoints++;
                                                   Result yieldResult = scheduler.yield();
                                                   if (not yieldResult)
                                                   {
                                                       state.sawCancel = scheduler.isCurrentTaskCancellationRequested();
                                                       return yieldResult;
                                                   }
                                               }
                                           })));

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Ready);
        SC_TEST_EXPECT(state.sawCurrent);
        SC_TEST_EXPECT(state.yieldPoints == 1);

        SC_TEST_EXPECT(scheduler.requestCancel(task));
        SC_TEST_EXPECT(task.isCancellationRequested());
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.sawCancel);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
    }

    void cancellationToken()
    {
        {
            struct State
            {
                bool sawCancel = false;
            };

            FiberScheduler               scheduler;
            FiberCancellationTokenSource tokenSource;
            FiberTask                    task;

            char       stackMemory[64 * 1024] = {};
            FiberStack stack({stackMemory, sizeof(stackMemory)});

            State state;
            SC_TEST_EXPECT(not tokenSource.isCancellationRequested());
            SC_TEST_EXPECT(tokenSource.token().isValid());
            SC_TEST_EXPECT(tokenSource.check());

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   for (;;)
                                                   {
                                                       Result yieldResult = scheduler.yield();
                                                       if (not yieldResult)
                                                       {
                                                           state.sawCancel =
                                                               scheduler.isCurrentTaskCancellationRequested();
                                                           return yieldResult;
                                                       }
                                                   }
                                               }),
                                           tokenSource.token()));

            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(task.status() == FiberTaskStatus::Ready);
            SC_TEST_EXPECT(scheduler.requestCancel(tokenSource));
            SC_TEST_EXPECT(tokenSource.isCancellationRequested());
            SC_TEST_EXPECT(not tokenSource.check());
            SC_TEST_EXPECT(task.isCancellationRequested());
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(state.sawCancel);
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());

            tokenSource.reset();
            SC_TEST_EXPECT(not tokenSource.isCancellationRequested());
        }

        {
            FiberScheduler               scheduler;
            FiberCancellationTokenSource tokenSource;
            FiberCounter                 counter;
            FiberTask                    waitingTask;

            char       stackMemory[64 * 1024] = {};
            FiberStack stack({stackMemory, sizeof(stackMemory)});

            scheduler.add(counter);
            SC_TEST_EXPECT(scheduler.spawn(waitingTask, stack,
                                           FiberTask::Procedure(
                                               [&counter](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(scheduler.wait(counter));
                                                   return Result(true);
                                               }),
                                           tokenSource.token()));

            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(waitingTask.status() == FiberTaskStatus::Waiting);
            SC_TEST_EXPECT(scheduler.requestCancel(tokenSource));
            SC_TEST_EXPECT(waitingTask.status() == FiberTaskStatus::Ready);
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(waitingTask.isCompleted());
            SC_TEST_EXPECT(not waitingTask.result());

            SC_TEST_EXPECT(counter.value() == 1);
            SC_TEST_EXPECT(scheduler.done(counter));
        }

        {
            FiberScheduler               scheduler;
            FiberCancellationTokenSource tokenSource;
            FiberTask                    task;

            char       stackMemory[64 * 1024] = {};
            FiberStack stack({stackMemory, sizeof(stackMemory)});

            tokenSource.requestCancel();
            SC_TEST_EXPECT(scheduler.spawn(
                task, stack, FiberTask::Procedure([](FiberScheduler& scheduler) { return scheduler.yield(); }),
                tokenSource.token()));
            SC_TEST_EXPECT(task.isCancellationRequested());
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
        }
    }

    void schedulerShutdown()
    {
        struct State
        {
            FiberCounter* counter          = nullptr;
            bool          yieldingCanceled = false;
            bool          waitingCanceled  = false;
        };

        FiberScheduler scheduler;
        FiberCounter   counter;
        FiberTask      yieldingTask;
        FiberTask      waitingTask;

        char       yieldingStackMemory[64 * 1024] = {};
        char       waitingStackMemory[64 * 1024]  = {};
        FiberStack yieldingStack({yieldingStackMemory, sizeof(yieldingStackMemory)});
        FiberStack waitingStack({waitingStackMemory, sizeof(waitingStackMemory)});

        State state;
        state.counter = &counter;

        scheduler.add(counter);
        SC_TEST_EXPECT(scheduler.spawn(yieldingTask, yieldingStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               for (;;)
                                               {
                                                   Result result = scheduler.yield();
                                                   if (not result)
                                                   {
                                                       state.yieldingCanceled = true;
                                                       return result;
                                                   }
                                               }
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(waitingTask, waitingStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               Result result = scheduler.wait(*state.counter);
                                               if (not result)
                                               {
                                                   state.waitingCanceled = true;
                                               }
                                               return result;
                                           })));

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(yieldingTask.status() == FiberTaskStatus::Ready);
        SC_TEST_EXPECT(waitingTask.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(scheduler.shutdown());
        SC_TEST_EXPECT(state.yieldingCanceled);
        SC_TEST_EXPECT(state.waitingCanceled);
        SC_TEST_EXPECT(yieldingTask.isCompleted());
        SC_TEST_EXPECT(waitingTask.isCompleted());
        SC_TEST_EXPECT(not yieldingTask.result());
        SC_TEST_EXPECT(not waitingTask.result());
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());

        SC_TEST_EXPECT(counter.value() == 1);
        SC_TEST_EXPECT(scheduler.done(counter));
    }

    void cancelWaitingTask()
    {
        FiberScheduler scheduler;
        FiberCounter   counter;
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        scheduler.add(counter);
        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&counter](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.wait(counter));
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(scheduler.requestCancel(task));
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Ready);
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());

        SC_TEST_EXPECT(counter.value() == 1);
        SC_TEST_EXPECT(scheduler.done(counter));
        SC_TEST_EXPECT(counter.value() == 0);
    }

    void uninterruptibleWaitCancellation()
    {
        struct State
        {
            FiberCounter* counter = nullptr;
            FiberTask*    waiter  = nullptr;

            bool resumed           = false;
            bool resumedBeforeDone = false;
        };

        FiberScheduler scheduler;
        FiberCounter   counter;
        FiberTask      waiter;
        FiberTask      canceller;

        char       waiterStackMemory[64 * 1024]    = {};
        char       cancellerStackMemory[64 * 1024] = {};
        FiberStack waiterStack({waiterStackMemory, sizeof(waiterStackMemory)});
        FiberStack cancellerStack({cancellerStackMemory, sizeof(cancellerStackMemory)});

        State state;
        state.counter = &counter;
        state.waiter  = &waiter;

        scheduler.add(counter);
        SC_TEST_EXPECT(scheduler.spawn(waiter, waiterStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.waitUninterruptible(*state.counter));
                                               state.resumed = true;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(canceller, cancellerStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.requestCancel(*state.waiter));
                                               SC_TRY(scheduler.yield());
                                               state.resumedBeforeDone = state.resumed;
                                               SC_TRY(scheduler.requestCancel(*state.waiter));
                                               SC_TRY(scheduler.done(*state.counter));
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(not state.resumedBeforeDone);
        SC_TEST_EXPECT(state.resumed);
        SC_TEST_EXPECT(counter.value() == 0);
        SC_TEST_EXPECT(waiter.isCompleted());
        SC_TEST_EXPECT(canceller.isCompleted());
        SC_TEST_EXPECT(waiter.result());
        SC_TEST_EXPECT(canceller.result());
    }

    void taskGroup()
    {
        struct State
        {
            int completed = 0;
        };

        FiberScheduler scheduler;
        FiberTask      firstTask;
        FiberTask      secondTask;

        char       firstStackMemory[64 * 1024]  = {};
        char       secondStackMemory[64 * 1024] = {};
        FiberStack firstStack({firstStackMemory, sizeof(firstStackMemory)});
        FiberStack secondStack({secondStackMemory, sizeof(secondStackMemory)});

        State          state;
        FiberTaskGroup group(scheduler);
        SC_TEST_EXPECT(group.spawn(firstTask, firstStack,
                                   FiberTask::Procedure(
                                       [&state](FiberScheduler& scheduler)
                                       {
                                           SC_TRY(scheduler.yield());
                                           state.completed++;
                                           return Result(true);
                                       })));
        SC_TEST_EXPECT(group.spawn(secondTask, secondStack,
                                   FiberTask::Procedure(
                                       [&state](FiberScheduler&)
                                       {
                                           state.completed++;
                                           return Result(true);
                                       })));
        SC_TEST_EXPECT(group.pending() == 2);
        SC_TEST_EXPECT(group.wait());
        SC_TEST_EXPECT(group.pending() == 0);
        SC_TEST_EXPECT(state.completed == 2);
        SC_TEST_EXPECT(firstTask.result());
        SC_TEST_EXPECT(secondTask.result());

        {
            struct GroupOptionsState
            {
                FiberTask* task      = nullptr;
                bool       sawTask   = false;
                int        completed = 0;
            };

            FiberScheduler optionsScheduler;
            FiberTask      optionsTask;
            char           optionsStackMemory[64 * 1024] = {};
            FiberStack     optionsStack({optionsStackMemory, sizeof(optionsStackMemory)});

            GroupOptionsState     optionsState;
            FiberTaskGroup        optionsGroup(optionsScheduler);
            FiberTaskSpawnOptions options;
            options.userData    = &optionsState;
            options.setUserData = true;
            SC_TEST_EXPECT(optionsGroup.spawn(optionsTask, optionsStack,
                                              FiberTask::Procedure(
                                                  [](FiberScheduler& scheduler)
                                                  {
                                                      FiberTask* current = scheduler.currentTask();
                                                      if (current == nullptr)
                                                      {
                                                          return Result::Error("Expected current fiber task");
                                                      }
                                                      GroupOptionsState* state =
                                                          static_cast<GroupOptionsState*>(current->userData());
                                                      if (state == nullptr)
                                                      {
                                                          return Result::Error("Expected task user data");
                                                      }
                                                      state->sawTask = current == state->task;
                                                      state->completed++;
                                                      return Result(true);
                                                  }),
                                              options));
            optionsState.task = &optionsTask;
            SC_TEST_EXPECT(optionsTask.userData() == &optionsState);
            SC_TEST_EXPECT(optionsGroup.waitAll());
            SC_TEST_EXPECT(optionsState.sawTask);
            SC_TEST_EXPECT(optionsState.completed == 1);
            SC_TEST_EXPECT(optionsTask.result());

            FiberTask     poolTasks[1];
            char          poolStackMemory[64 * 1024] = {};
            FiberTaskPool pool({poolTasks, 1}, {poolStackMemory, sizeof(poolStackMemory)}, 64 * 1024);
            FiberTask*    spawnedTask = nullptr;
            SC_TEST_EXPECT(optionsGroup.spawn(pool,
                                              FiberTask::Procedure(
                                                  [](FiberScheduler& scheduler)
                                                  {
                                                      FiberTask* current = scheduler.currentTask();
                                                      if (current == nullptr)
                                                      {
                                                          return Result::Error("Expected current fiber task");
                                                      }
                                                      GroupOptionsState* state =
                                                          static_cast<GroupOptionsState*>(current->userData());
                                                      if (state == nullptr)
                                                      {
                                                          return Result::Error("Expected task user data");
                                                      }
                                                      state->sawTask = current == state->task;
                                                      state->completed++;
                                                      return Result(true);
                                                  }),
                                              options, &spawnedTask));
            optionsState.task = spawnedTask;
            SC_TEST_EXPECT(spawnedTask != nullptr);
            if (spawnedTask != nullptr)
            {
                SC_TEST_EXPECT(spawnedTask->userData() == &optionsState);
                SC_TEST_EXPECT(optionsGroup.waitAll());
                SC_TEST_EXPECT(spawnedTask->result());
            }
            SC_TEST_EXPECT(optionsState.sawTask);
            SC_TEST_EXPECT(optionsState.completed == 2);

            FiberCounter invalidCounter;
            options.counter       = &invalidCounter;
            Result invalidOptions = optionsGroup.spawn(
                optionsTask, optionsStack, FiberTask::Procedure([](FiberScheduler&) { return Result(true); }), options);
            SC_TEST_EXPECT(not invalidOptions);
        }

        {
            struct CancelState
            {
                FiberTask*  childTask         = nullptr;
                FiberStack* childStack        = nullptr;
                bool        parentEnteredWait = false;
                bool        parentCanceled    = false;
                bool        childCanceled     = false;
            };

            FiberScheduler cancelScheduler;
            FiberTask      parentTask;
            FiberTask      childTask;

            char       parentStackMemory[64 * 1024] = {};
            char       childStackMemory[64 * 1024]  = {};
            FiberStack parentStack({parentStackMemory, sizeof(parentStackMemory)});
            FiberStack childStack({childStackMemory, sizeof(childStackMemory)});

            CancelState cancelState;
            cancelState.childTask  = &childTask;
            cancelState.childStack = &childStack;

            SC_TEST_EXPECT(
                cancelScheduler.spawn(parentTask, parentStack,
                                      FiberTask::Procedure(
                                          [&cancelState](FiberScheduler& scheduler)
                                          {
                                              FiberTaskGroup childGroup(scheduler);
                                              SC_TRY(childGroup.spawn(*cancelState.childTask, *cancelState.childStack,
                                                                      FiberTask::Procedure(
                                                                          [&cancelState](FiberScheduler& scheduler)
                                                                          {
                                                                              for (;;)
                                                                              {
                                                                                  Result result = scheduler.yield();
                                                                                  if (not result)
                                                                                  {
                                                                                      cancelState.childCanceled = true;
                                                                                      return result;
                                                                                  }
                                                                              }
                                                                          })));

                                              cancelState.parentEnteredWait = true;
                                              Result result                 = childGroup.waitAllCancelOnParentCancel();
                                              if (not result)
                                              {
                                                  cancelState.parentCanceled = true;
                                              }
                                              return result;
                                          })));

            SC_TEST_EXPECT(cancelScheduler.runOnce());
            SC_TEST_EXPECT(cancelState.parentEnteredWait);
            SC_TEST_EXPECT(parentTask.status() == FiberTaskStatus::Waiting);
            SC_TEST_EXPECT(cancelScheduler.requestCancel(parentTask));
            SC_TEST_EXPECT(cancelScheduler.run());
            SC_TEST_EXPECT(cancelState.parentCanceled);
            SC_TEST_EXPECT(cancelState.childCanceled);
            SC_TEST_EXPECT(parentTask.isCompleted());
            SC_TEST_EXPECT(childTask.isCompleted());
            SC_TEST_EXPECT(not parentTask.result());
            SC_TEST_EXPECT(not childTask.result());
            SC_TEST_EXPECT(not cancelScheduler.hasActiveFibers());
        }
    }

    void taskGroupErrors()
    {
        {
            struct State
            {
                bool longTaskCanceled = false;
            };

            FiberScheduler scheduler;
            FiberTask      longTask;
            FiberTask      failingTask;

            char       longStackMemory[64 * 1024]    = {};
            char       failingStackMemory[64 * 1024] = {};
            FiberStack longStack({longStackMemory, sizeof(longStackMemory)});
            FiberStack failingStack({failingStackMemory, sizeof(failingStackMemory)});

            State          state;
            FiberTaskGroup group(scheduler);
            SC_TEST_EXPECT(group.spawn(longTask, longStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               for (;;)
                                               {
                                                   Result yieldResult = scheduler.yield();
                                                   if (not yieldResult)
                                                   {
                                                       state.longTaskCanceled = true;
                                                       return yieldResult;
                                                   }
                                               }
                                           })));
            SC_TEST_EXPECT(group.spawn(failingTask, failingStack,
                                       FiberTask::Procedure(
                                           [](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.yield());
                                               return Result::Error("Expected task group failure");
                                           })));

            Result firstError(true);
            Result waitResult = group.waitCancelOnError(&firstError);
            SC_TEST_EXPECT(not waitResult);
            SC_TEST_EXPECT(not firstError);
            SC_TEST_EXPECT(state.longTaskCanceled);
            SC_TEST_EXPECT(longTask.isCompleted());
            SC_TEST_EXPECT(failingTask.isCompleted());
            SC_TEST_EXPECT(group.pending() == 0);
        }

        {
            FiberScheduler scheduler;
            FiberTask      firstFailingTask;
            FiberTask      secondFailingTask;
            FiberTask      successfulTask;

            char       firstFailingStackMemory[64 * 1024]  = {};
            char       secondFailingStackMemory[64 * 1024] = {};
            char       successfulStackMemory[64 * 1024]    = {};
            FiberStack firstFailingStack({firstFailingStackMemory, sizeof(firstFailingStackMemory)});
            FiberStack secondFailingStack({secondFailingStackMemory, sizeof(secondFailingStackMemory)});
            FiberStack successfulStack({successfulStackMemory, sizeof(successfulStackMemory)});

            FiberTaskGroup group(scheduler);
            SC_TEST_EXPECT(group.spawn(
                firstFailingTask, firstFailingStack,
                FiberTask::Procedure([](FiberScheduler&) { return Result::Error("Expected first waitAll failure"); })));
            SC_TEST_EXPECT(group.spawn(successfulTask, successfulStack,
                                       FiberTask::Procedure([](FiberScheduler&) { return Result(true); })));
            SC_TEST_EXPECT(group.spawn(secondFailingTask, secondFailingStack,
                                       FiberTask::Procedure(
                                           [](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.yield());
                                               return Result::Error("Expected second waitAll failure");
                                           })));

            Result firstError(true);
            Result waitResult = group.waitAll(&firstError);
            SC_TEST_EXPECT(not waitResult);
            SC_TEST_EXPECT(not firstError);
            SC_TEST_EXPECT(firstFailingTask.isCompleted());
            SC_TEST_EXPECT(secondFailingTask.isCompleted());
            SC_TEST_EXPECT(successfulTask.isCompleted());
            SC_TEST_EXPECT(group.pending() == 0);
            SC_TEST_EXPECT(group.countErrors() == 2);

            FiberTaskGroupError errors[2];
            size_t              numErrors = 0;
            SC_TEST_EXPECT(group.collectErrors(errors, numErrors));
            SC_TEST_EXPECT(numErrors == 2);

            bool foundFirstError  = false;
            bool foundSecondError = false;
            for (const FiberTaskGroupError& error : errors)
            {
                if (error.task == &firstFailingTask)
                {
                    foundFirstError = true;
                    SC_TEST_EXPECT(error.result.message == firstFailingTask.result().message);
                }
                if (error.task == &secondFailingTask)
                {
                    foundSecondError = true;
                    SC_TEST_EXPECT(error.result.message == secondFailingTask.result().message);
                }
            }
            SC_TEST_EXPECT(foundFirstError);
            SC_TEST_EXPECT(foundSecondError);

            FiberTaskGroupError smallErrors[1];
            numErrors          = 0;
            Result smallResult = group.collectErrors(smallErrors, numErrors);
            SC_TEST_EXPECT(not smallResult);
            SC_TEST_EXPECT(numErrors == 1);
        }

        {
            FiberScheduler scheduler;
            FiberTask      tasks[2];
            char           stackMemory[2 * 64 * 1024] = {};
            FiberTaskPool  pool({tasks, 2}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
            FiberTaskGroup group(scheduler);

            SC_TEST_EXPECT(group.spawn(pool, FiberTask::Procedure(
                                                 [](FiberScheduler& scheduler)
                                                 {
                                                     SC_TRY(scheduler.yield());
                                                     return Result(true);
                                                 })));
            SC_TEST_EXPECT(group.spawn(pool, FiberTask::Procedure(
                                                 [](FiberScheduler& scheduler)
                                                 {
                                                     SC_TRY(scheduler.yield());
                                                     return Result(true);
                                                 })));
            FiberTask* noSlotTask = &tasks[0];
            Result     noSlot =
                group.spawn(pool, FiberTask::Procedure([](FiberScheduler&) { return Result(true); }), &noSlotTask);
            SC_TEST_EXPECT(not noSlot);
            SC_TEST_EXPECT(noSlotTask == nullptr);
            SC_TEST_EXPECT(group.waitAll());
        }
    }

    void taskPool()
    {
        struct State
        {
            int completed = 0;
        };

        FiberScheduler scheduler;
        FiberTask      tasks[2];
        char           stackMemory[2 * 64 * 1024] = {};
        FiberTaskPool  pool({tasks, 2}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);

        State state;
        SC_TEST_EXPECT(pool.capacity() == 2);
        SC_TEST_EXPECT(pool.activeCount() == 0);
        SC_TEST_EXPECT(pool.availableCount() == 2);
        SC_TEST_EXPECT(pool.stackSizeInBytes() == 64 * 1024);

        FiberTaskPoolDiagnostics diagnostics;
        pool.diagnostics(diagnostics);
        SC_TEST_EXPECT(not diagnostics.classBacked);
        SC_TEST_EXPECT(diagnostics.capacity == 2);
        SC_TEST_EXPECT(diagnostics.activeTasks == 0);
        SC_TEST_EXPECT(diagnostics.availableTasks == 2);
        SC_TEST_EXPECT(diagnostics.taskClass.capacity == 2);
        SC_TEST_EXPECT(diagnostics.stackClass.capacity == 2);
        SC_TEST_EXPECT(diagnostics.stackClass.committedSizeBytes == sizeof(stackMemory));
        pool.fillHighWaterMarks();
        SC_TEST_EXPECT(pool.spawn(scheduler, FiberTask::Procedure(
                                                 [&state](FiberScheduler& scheduler)
                                                 {
                                                     SC_TRY(scheduler.yield());
                                                     state.completed++;
                                                     return Result(true);
                                                 })));
        SC_TEST_EXPECT(pool.activeCount() == 1);
        SC_TEST_EXPECT(pool.availableCount() == 1);
        SC_TEST_EXPECT(pool.spawn(scheduler, FiberTask::Procedure(
                                                 [&state](FiberScheduler&)
                                                 {
                                                     state.completed++;
                                                     return Result(true);
                                                 })));
        SC_TEST_EXPECT(pool.activeCount() == 2);
        SC_TEST_EXPECT(pool.availableCount() == 0);
        pool.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.activeTasks == 2);
        SC_TEST_EXPECT(diagnostics.availableTasks == 0);

        FiberTask* fullPoolTask = &tasks[0];
        Result     fullPool =
            pool.spawn(scheduler, FiberTask::Procedure([](FiberScheduler&) { return Result(true); }), &fullPoolTask);
        SC_TEST_EXPECT(not fullPool);
        SC_TEST_EXPECT(fullPoolTask == nullptr);
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.completed == 2);
        SC_TEST_EXPECT(pool.activeCount() == 0);
        SC_TEST_EXPECT(pool.availableCount() == 2);

        size_t firstStackUsed = 0;
        SC_TEST_EXPECT(pool.stackHighWaterUsedBytes(0, firstStackUsed));
        SC_TEST_EXPECT(firstStackUsed > 0);
        SC_TEST_EXPECT(firstStackUsed < pool.stackSizeInBytes());

        SC_TEST_EXPECT(pool.spawn(scheduler, FiberTask::Procedure(
                                                 [&state](FiberScheduler&)
                                                 {
                                                     state.completed++;
                                                     return Result(true);
                                                 })));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.completed == 3);

        FiberTask     rotatingTasks[3];
        char          rotatingStackMemory[3 * 64 * 1024] = {};
        FiberTaskPool rotatingPool({rotatingTasks, 3}, {rotatingStackMemory, sizeof(rotatingStackMemory)}, 64 * 1024);
        FiberTask*    spawnedTask       = nullptr;
        int           rotatingCompleted = 0;

        SC_TEST_EXPECT(rotatingPool.spawn(scheduler,
                                          FiberTask::Procedure(
                                              [&rotatingCompleted](FiberScheduler&)
                                              {
                                                  rotatingCompleted++;
                                                  return Result(true);
                                              }),
                                          &spawnedTask));
        SC_TEST_EXPECT(spawnedTask == &rotatingTasks[0]);
        SC_TEST_EXPECT(rotatingPool.spawn(scheduler,
                                          FiberTask::Procedure(
                                              [&rotatingCompleted](FiberScheduler&)
                                              {
                                                  rotatingCompleted++;
                                                  return Result(true);
                                              }),
                                          &spawnedTask));
        SC_TEST_EXPECT(spawnedTask == &rotatingTasks[1]);
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(rotatingCompleted == 2);

        SC_TEST_EXPECT(rotatingPool.spawn(scheduler,
                                          FiberTask::Procedure(
                                              [&rotatingCompleted](FiberScheduler&)
                                              {
                                                  rotatingCompleted++;
                                                  return Result(true);
                                              }),
                                          &spawnedTask));
        SC_TEST_EXPECT(spawnedTask == &rotatingTasks[2]);
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(rotatingCompleted == 3);
    }

    void taskPoolAvailabilityWait()
    {
        struct State
        {
            int completed = 0;
            int waited    = 0;
        };

        FiberScheduler scheduler;
        FiberTask      poolTasks[2];
        char           poolStacks[2 * 64 * 1024] = {};
        FiberTaskPool  pool({poolTasks, 2}, {poolStacks, sizeof(poolStacks)}, 64 * 1024);
        FiberTask      producerTask;
        char           producerStackMemory[64 * 1024] = {};
        FiberStack     producerStack({producerStackMemory, sizeof(producerStackMemory)});
        State          state;

        SC_TEST_EXPECT(scheduler.spawn(producerTask, producerStack,
                                       FiberTask::Procedure(
                                           [&pool, &state](FiberScheduler& scheduler)
                                           {
                                               if (not pool.hasAvailableTask())
                                               {
                                                   return Result::Error("FiberTaskPool should start available");
                                               }
                                               for (size_t idx = 0; idx < 2; ++idx)
                                               {
                                                   SC_TRY(pool.spawn(scheduler, FiberTask::Procedure(
                                                                                    [&state](FiberScheduler&)
                                                                                    {
                                                                                        state.completed++;
                                                                                        return Result(true);
                                                                                    })));
                                               }
                                               if (pool.hasAvailableTask())
                                               {
                                                   return Result::Error("FiberTaskPool should be full");
                                               }

                                               SC_TRY(pool.waitForAvailableTask(scheduler));
                                               if (not pool.hasAvailableTask())
                                               {
                                                   return Result::Error("FiberTaskPool should have a completed slot");
                                               }
                                               state.waited++;
                                               SC_TRY(pool.spawn(scheduler, FiberTask::Procedure(
                                                                                [&state](FiberScheduler&)
                                                                                {
                                                                                    state.completed++;
                                                                                    return Result(true);
                                                                                })));
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(producerTask.result());
        SC_TEST_EXPECT(state.waited == 1);
        SC_TEST_EXPECT(state.completed == 3);
        SC_TEST_EXPECT(pool.availableCount() == 2);
    }

    void taskPoolAvailabilityCancellation()
    {
        struct State
        {
            int childCompleted    = 0;
            int producerCancelled = 0;
        };

        FiberScheduler scheduler;
        FiberTask      poolTasks[1];
        char           poolStacks[64 * 1024] = {};
        FiberTaskPool  pool({poolTasks, 1}, {poolStacks, sizeof(poolStacks)}, 64 * 1024);
        FiberCounter   blocker;
        FiberTask      producerTask;
        FiberTask      cancellerTask;
        char           producerStackMemory[64 * 1024]  = {};
        char           cancellerStackMemory[64 * 1024] = {};
        FiberStack     producerStack({producerStackMemory, sizeof(producerStackMemory)});
        FiberStack     cancellerStack({cancellerStackMemory, sizeof(cancellerStackMemory)});
        State          state;
        struct Context
        {
            FiberTaskPool* pool         = nullptr;
            FiberCounter*  blocker      = nullptr;
            FiberTask*     producerTask = nullptr;
            State*         state        = nullptr;
        };
        Context context = {&pool, &blocker, &producerTask, &state};

        scheduler.add(blocker);
        SC_TEST_EXPECT(
            scheduler.spawn(producerTask, producerStack,
                            FiberTask::Procedure(
                                [&context](FiberScheduler& scheduler)
                                {
                                    SC_TRY(context.pool->spawn(scheduler, FiberTask::Procedure(
                                                                              [&context](FiberScheduler& scheduler)
                                                                              {
                                                                                  SC_TRY(scheduler.waitUninterruptible(
                                                                                      *context.blocker));
                                                                                  context.state->childCompleted++;
                                                                                  return Result(true);
                                                                              })));
                                    if (context.pool->hasAvailableTask())
                                    {
                                        return Result::Error("FiberTaskPool should be full");
                                    }

                                    Result waitResult = context.pool->waitForAvailableTask(scheduler);
                                    if (waitResult)
                                    {
                                        return Result::Error("FiberTaskPool wait should be cancelled");
                                    }
                                    context.state->producerCancelled++;
                                    return Result(true);
                                })));
        SC_TEST_EXPECT(scheduler.spawn(cancellerTask, cancellerStack,
                                       FiberTask::Procedure(
                                           [&context](FiberScheduler& runningScheduler)
                                           {
                                               SC_TRY(runningScheduler.yield());
                                               SC_TRY(runningScheduler.requestCancel(*context.producerTask));
                                               SC_TRY(runningScheduler.done(*context.blocker));
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(producerTask.result());
        SC_TEST_EXPECT(cancellerTask.result());
        SC_TEST_EXPECT(poolTasks[0].result());
        SC_TEST_EXPECT(state.producerCancelled == 1);
        SC_TEST_EXPECT(state.childCompleted == 1);
        SC_TEST_EXPECT(pool.availableCount() == 1);
    }

    void taskPoolAvailabilityWorkerPool()
    {
        static constexpr size_t NumWorkers = 4;
        static constexpr size_t NumTasks   = 2;
        static constexpr size_t NumJobs    = 6;

        struct State
        {
            Atomic<int32_t> completed;
            Atomic<int32_t> producerDone;
        };

        FiberScheduler    scheduler;
        FiberTask         poolTasks[NumTasks];
        static char       poolStacks[NumTasks * 64 * 1024] = {};
        FiberTaskPool     pool({poolTasks, NumTasks}, {poolStacks, sizeof(poolStacks)}, 64 * 1024);
        FiberTask         producerTask;
        static char       producerStackMemory[64 * 1024] = {};
        FiberStack        producerStack({producerStackMemory, sizeof(producerStackMemory)});
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;
        State             state;

        SC_TEST_EXPECT(scheduler.spawn(producerTask, producerStack,
                                       FiberTask::Procedure(
                                           [&pool, &state](FiberScheduler& scheduler)
                                           {
                                               for (size_t idx = 0; idx < NumJobs; ++idx)
                                               {
                                                   while (not pool.hasAvailableTask())
                                                   {
                                                       SC_TRY(pool.waitForAvailableTask(scheduler));
                                                   }
                                                   SC_TRY(pool.spawn(scheduler, FiberTask::Procedure(
                                                                                    [&state](FiberScheduler& scheduler)
                                                                                    {
                                                                                        for (int loop = 0; loop < 8;
                                                                                             ++loop)
                                                                                        {
                                                                                            SC_TRY(scheduler.yield());
                                                                                        }
                                                                                        state.completed.fetch_add(1);
                                                                                        return Result(true);
                                                                                    })));
                                               }
                                               state.producerDone.fetch_add(1);
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(producerTask.result());
        SC_TEST_EXPECT(state.producerDone.load() == 1);
        SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumJobs));
        SC_TEST_EXPECT(pool.availableCount() == NumTasks);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
    }

    void taskPoolWorkerReusePressure()
    {
        static constexpr size_t NumWorkers = 4;
        static constexpr size_t NumTasks   = 2;
        static constexpr size_t NumJobs    = 256;
        static constexpr size_t StackSize  = 32 * 1024;

        struct State
        {
            Atomic<int32_t> submitted;
            Atomic<int32_t> completed;
            Atomic<int32_t> checksum;
        };

        FiberScheduler    scheduler;
        FiberTask         poolTasks[NumTasks];
        static char       poolStacks[NumTasks * StackSize] = {};
        FiberTaskPool     pool({poolTasks, NumTasks}, {poolStacks, sizeof(poolStacks)}, StackSize);
        FiberTask         producerTask;
        static char       producerStackMemory[StackSize] = {};
        FiberStack        producerStack({producerStackMemory, sizeof(producerStackMemory)});
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;
        State             state;

        SC_TEST_EXPECT(scheduler.spawn(producerTask, producerStack,
                                       FiberTask::Procedure(
                                           [&pool, &state](FiberScheduler& scheduler)
                                           {
                                               for (size_t idx = 0; idx < NumJobs; ++idx)
                                               {
                                                   while (not pool.hasAvailableTask())
                                                   {
                                                       SC_TRY(pool.waitForAvailableTask(scheduler));
                                                   }
                                                   SC_TRY(pool.spawn(
                                                       scheduler, FiberTask::Procedure(
                                                                      [&state, idx](FiberScheduler&)
                                                                      {
                                                                          int32_t value = 0;
                                                                          for (int32_t loop = 0; loop < 8; ++loop)
                                                                          {
                                                                              value += static_cast<int32_t>(idx) + loop;
                                                                          }
                                                                          state.checksum.fetch_add(value);
                                                                          state.completed.fetch_add(1);
                                                                          return Result(true);
                                                                      })));
                                                   state.submitted.fetch_add(1);
                                               }
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(producerTask.result());
        SC_TEST_EXPECT(state.submitted.load() == static_cast<int32_t>(NumJobs));
        SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumJobs));
        SC_TEST_EXPECT(state.checksum.load() == 268288);
        SC_TEST_EXPECT(pool.availableCount() == NumTasks);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
    }

    void taskPoolSustainedStealingStress()
    {
        static constexpr size_t NumWorkers             = 4;
        static constexpr size_t NumTaskSlots           = 32;
        static constexpr size_t NumJobs                = 512;
        static constexpr size_t StackSize              = 32 * 1024;
        static constexpr size_t DequeCapacityPerWorker = 16;

        struct State
        {
            Atomic<int32_t>  nextJob;
            Atomic<int32_t>  submitted;
            Atomic<int32_t>  completed;
            Atomic<int32_t>* runCounts = nullptr;
        };

        FiberScheduler         scheduler;
        FiberAllocator         allocator;
        static char            dequeStorage[NumWorkers * DequeCapacityPerWorker * sizeof(FiberTask*) + 4096] = {};
        static FiberTask       poolTasks[NumTaskSlots];
        static char            poolStacks[NumTaskSlots * StackSize] = {};
        FiberTaskPool          pool({poolTasks, NumTaskSlots}, {poolStacks, sizeof(poolStacks)}, StackSize);
        FiberTask              producerTask;
        static char            producerStackMemory[StackSize] = {};
        FiberStack             producerStack({producerStackMemory, sizeof(producerStackMemory)});
        FiberWorker            workers[NumWorkers];
        FiberWorkerThread      threads[NumWorkers];
        FiberWorkerPool        workerPool;
        State                  state;
        static Atomic<int32_t> runCounts[NumJobs];

        for (Atomic<int32_t>& runCount : runCounts)
        {
            runCount.store(0);
        }
        state.runCounts = runCounts;

        FiberWorkerPoolOptions workerPoolOptions;
        workerPoolOptions.dequeAllocator         = &allocator;
        workerPoolOptions.dequeCapacityPerWorker = DequeCapacityPerWorker;

        SC_TEST_EXPECT(allocator.createFixed(dequeStorage));
        SC_TEST_EXPECT(scheduler.spawn(producerTask, producerStack,
                                       FiberTask::Procedure(
                                           [&pool, &state](FiberScheduler& scheduler)
                                           {
                                               for (;;)
                                               {
                                                   while (not pool.hasAvailableTask())
                                                   {
                                                       SC_TRY(pool.waitForAvailableTask(scheduler));
                                                   }

                                                   const int32_t jobIndex = state.nextJob.load();
                                                   if (jobIndex >= static_cast<int32_t>(NumJobs))
                                                   {
                                                       return Result(true);
                                                   }

                                                   Result spawnResult = pool.spawn(
                                                       scheduler, FiberTask::Procedure(
                                                                      [&state, jobIndex](FiberScheduler& scheduler)
                                                                      {
                                                                          for (int loop = 0; loop < 3; ++loop)
                                                                          {
                                                                              SC_TRY(scheduler.yield());
                                                                          }
                                                                          state.runCounts[jobIndex].fetch_add(1);
                                                                          state.completed.fetch_add(1);
                                                                          return Result(true);
                                                                      }));
                                                   if (not spawnResult)
                                                   {
                                                       SC_TRY(pool.waitForAvailableTask(scheduler));
                                                       continue;
                                                   }
                                                   state.nextJob.fetch_add(1);
                                                   state.submitted.fetch_add(1);
                                               }
                                           })));

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, workerPoolOptions));
        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(producerTask.result());
        SC_TEST_EXPECT(state.submitted.load() == static_cast<int32_t>(NumJobs));
        SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumJobs));
        for (Atomic<int32_t>& runCount : runCounts)
        {
            SC_TEST_EXPECT(runCount.load() == 1);
        }

        SC_TEST_EXPECT(pool.availableCount() == NumTaskSlots);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(allocator.close());

        {
            static constexpr size_t NumStealJobs       = 128;
            static constexpr size_t StealDequeCapacity = NumStealJobs;

            FiberScheduler         stealScheduler;
            FiberAllocator         stealAllocator;
            static char            stealDequeStorage[NumWorkers * StealDequeCapacity * sizeof(FiberTask*) + 4096] = {};
            static FiberTask       stealTasks[NumStealJobs];
            static char            stealStacks[NumStealJobs * StackSize] = {};
            FiberTaskPool          stealPool({stealTasks, NumStealJobs}, {stealStacks, sizeof(stealStacks)}, StackSize);
            FiberWorker            stealWorkers[NumWorkers];
            FiberCounter           gate;
            static Atomic<int32_t> stealRunCounts[NumStealJobs];

            for (Atomic<int32_t>& runCount : stealRunCounts)
            {
                runCount.store(0);
            }

            SC_TEST_EXPECT(stealAllocator.createFixed(stealDequeStorage));
            SC_TEST_EXPECT(
                stealScheduler.createWorkerDeques(stealAllocator, {stealWorkers, NumWorkers}, StealDequeCapacity));
            stealScheduler.add(gate);
            for (size_t idx = 0; idx < NumStealJobs; ++idx)
            {
                SC_TEST_EXPECT(stealPool.spawn(stealScheduler, FiberTask::Procedure(
                                                                   [idx, &gate](FiberScheduler& runningScheduler)
                                                                   {
                                                                       SC_TRY(runningScheduler.wait(gate));
                                                                       stealRunCounts[idx].fetch_add(1);
                                                                       return Result(true);
                                                                   })));
            }
            for (size_t idx = 0; idx < NumStealJobs; ++idx)
            {
                SC_TEST_EXPECT(stealScheduler.runNoWait(stealWorkers[0], {stealWorkers, NumWorkers}));
                SC_TEST_EXPECT(stealTasks[idx].status() == FiberTaskStatus::Waiting);
            }

            SC_TEST_EXPECT(stealScheduler.done(gate));
            SC_TEST_EXPECT(stealScheduler.readyFiberCount() == NumStealJobs);
            SC_TEST_EXPECT(stealScheduler.readyFiberCount(stealWorkers[0]) == 0);
            for (size_t idx = 0; idx < NumStealJobs; ++idx)
            {
                FiberWorker& thief = stealWorkers[1 + (idx % (NumWorkers - 1))];
                SC_TEST_EXPECT(stealScheduler.runNoWait(thief, {stealWorkers, NumWorkers}));
            }

            for (Atomic<int32_t>& runCount : stealRunCounts)
            {
                SC_TEST_EXPECT(runCount.load() == 1);
            }
            FiberWorkerDiagnostics stealDiagnostics;
            stealScheduler.workerDiagnostics({stealWorkers, NumWorkers}, stealDiagnostics);
            SC_TEST_EXPECT(stealDiagnostics.stolenFibers == 0);
            SC_TEST_EXPECT(stealPool.availableCount() == NumStealJobs);
            SC_TEST_EXPECT(not stealScheduler.hasActiveFibers());
            stealScheduler.releaseWorkerDeques({stealWorkers, NumWorkers});
            SC_TEST_EXPECT(stealAllocator.close());
        }
    }

    void taskPoolAvailabilityExternalProducer()
    {
        struct State
        {
            int completed = 0;
        };

        FiberScheduler scheduler;
        FiberTask      poolTasks[1];
        char           poolStacks[64 * 1024] = {};
        FiberTaskPool  pool({poolTasks, 1}, {poolStacks, sizeof(poolStacks)}, 64 * 1024);
        State          state;

        SC_TEST_EXPECT(pool.spawn(scheduler, FiberTask::Procedure(
                                                 [&state](FiberScheduler& scheduler)
                                                 {
                                                     SC_TRY(scheduler.yield());
                                                     state.completed++;
                                                     return Result(true);
                                                 })));
        SC_TEST_EXPECT(not pool.hasAvailableTask());

        SC_TEST_EXPECT(pool.waitForAvailableTask(scheduler));
        SC_TEST_EXPECT(state.completed == 1);
        SC_TEST_EXPECT(pool.hasAvailableTask());
        SC_TEST_EXPECT(pool.spawn(scheduler, FiberTask::Procedure(
                                                 [&state](FiberScheduler&)
                                                 {
                                                     state.completed++;
                                                     return Result(true);
                                                 })));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.completed == 2);
        SC_TEST_EXPECT(pool.availableCount() == 1);
    }

    void workerPool()
    {
        static constexpr size_t NumWorkers = 4;
        static constexpr size_t NumTasks   = 16;
        static constexpr int    NumYields  = 8;

        {
            FiberScheduler    scheduler;
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread threads[NumWorkers];
            FiberWorkerPool   workerPool;

            Result noWorkers = workerPool.start(scheduler, {}, {threads, NumWorkers});
            SC_TEST_EXPECT(not noWorkers);
            SC_TEST_EXPECT(not workerPool.isRunning());

            Result noThreads = workerPool.start(scheduler, {workers, NumWorkers}, {});
            SC_TEST_EXPECT(not noThreads);
            SC_TEST_EXPECT(not workerPool.isRunning());

            Result mismatch = workerPool.start(scheduler, {workers, NumWorkers - 1}, {threads, NumWorkers});
            SC_TEST_EXPECT(not mismatch);
            SC_TEST_EXPECT(not workerPool.isRunning());
            for (FiberWorkerThread& thread : threads)
            {
                SC_TEST_EXPECT(not thread.wasStarted());
            }

            uint64_t               mismatchedAffinityMasks[1] = {1};
            FiberWorkerPoolOptions badAffinityOptions;
            FiberWorkerThread      affinityThreads[NumWorkers];
            FiberWorkerPool        affinityPool;
            badAffinityOptions.affinityMasks = {mismatchedAffinityMasks, 1};
            Result badAffinity =
                affinityPool.start(scheduler, {workers, NumWorkers}, {affinityThreads, NumWorkers}, badAffinityOptions);
            SC_TEST_EXPECT(not badAffinity);
            SC_TEST_EXPECT(not affinityPool.isRunning());

            FiberWorkerPoolOptions priorityOptions;
            FiberWorkerThread      priorityThreads[NumWorkers];
            FiberWorkerPool        priorityPool;
            priorityOptions.threadPriority = FiberWorkerThreadPriority::High;
            Result priorityStart =
                priorityPool.start(scheduler, {workers, NumWorkers}, {priorityThreads, NumWorkers}, priorityOptions);
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(priorityStart);
            SC_TEST_EXPECT(priorityPool.join());
#else
            SC_TEST_EXPECT(not priorityStart);
            SC_TEST_EXPECT(not priorityPool.isRunning());
#endif
        }

        {
            struct State
            {
                Atomic<int32_t> completed;
            };

            FiberScheduler    scheduler;
            FiberTask         tasks[NumTasks];
            static char       stackMemory[NumTasks * 64 * 1024] = {};
            FiberTaskPool     taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread threads[NumWorkers];
            FiberWorkerPool   workerPool;
            State             state;

            for (size_t idx = 0; idx < NumTasks; ++idx)
            {
                SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                             [&state](FiberScheduler& scheduler)
                                                             {
                                                                 for (int loop = 0; loop < NumYields; ++loop)
                                                                 {
                                                                     SC_TRY(scheduler.yield());
                                                                 }
                                                                 state.completed.fetch_add(1);
                                                                 return Result(true);
                                                             })));
            }

            Result startResult = workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers});
            SC_TEST_EXPECT(startResult);
            SC_TEST_EXPECT(workerPool.isRunning());
            SC_TEST_EXPECT(workerPool.workerCount() == NumWorkers);
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(not workerPool.isRunning());
            SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumTasks));
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            for (size_t idx = 0; idx < NumTasks; ++idx)
            {
                SC_TEST_EXPECT(tasks[idx].result());
            }
        }

        {
            struct State
            {
                Atomic<int32_t> iterations;
                bool            canceled = false;
            };

            FiberScheduler    scheduler;
            FiberTask         task;
            char              stackMemory[64 * 1024] = {};
            FiberStack        stack({stackMemory, sizeof(stackMemory)});
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread threads[NumWorkers];
            FiberWorkerPool   workerPool;
            State             state;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   while (true)
                                                   {
                                                       Result result = scheduler.yield();
                                                       state.iterations.fetch_add(1);
                                                       if (not result)
                                                       {
                                                           state.canceled = true;
                                                           return result;
                                                       }
                                                   }
                                               })));

            SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
            for (int loop = 0; loop < 1000 and state.iterations.load() < 4; ++loop)
            {
                Thread::Sleep(1);
            }
            SC_TEST_EXPECT(state.iterations.load() >= 4);
            SC_TEST_EXPECT(workerPool.shutdown());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                Atomic<int32_t> step;
                FiberCounter*   counter = nullptr;
            };

            FiberScheduler    scheduler;
            FiberCounter      counter;
            FiberTask         task;
            char              stackMemory[64 * 1024] = {};
            FiberStack        stack({stackMemory, sizeof(stackMemory)});
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread threads[NumWorkers];
            FiberWorkerPool   workerPool;
            State             state;
            state.counter = &counter;

            scheduler.add(counter);
            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   state.step.store(1);
                                                   SC_TRY(scheduler.wait(*state.counter));
                                                   state.step.store(2);
                                                   return Result(true);
                                               })));

            SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
            for (int loop = 0; loop < 1000 and state.step.load() < 1; ++loop)
            {
                Thread::Sleep(1);
            }
            SC_TEST_EXPECT(state.step.load() == 1);
            SC_TEST_EXPECT(scheduler.done(counter));
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(state.step.load() == 2);
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                Atomic<int32_t> step;
                FiberCounter*   counter = nullptr;
            };

            FiberScheduler    scheduler;
            FiberCounter      counter;
            FiberTask         task;
            char              stackMemory[64 * 1024] = {};
            FiberStack        stack({stackMemory, sizeof(stackMemory)});
            FiberWorker       firstWorkers[NumWorkers];
            FiberWorkerThread firstThreads[NumWorkers];
            FiberWorkerPool   firstPool;
            FiberWorker       secondWorkers[NumWorkers];
            FiberWorkerThread secondThreads[NumWorkers];
            FiberWorkerPool   secondPool;
            State             state;
            state.counter = &counter;

            scheduler.add(counter);
            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   state.step.store(1);
                                                   SC_TRY(scheduler.wait(*state.counter));
                                                   state.step.store(2);
                                                   return Result(true);
                                               })));

            SC_TEST_EXPECT(firstPool.start(scheduler, {firstWorkers, NumWorkers}, {firstThreads, NumWorkers}));
            for (int loop = 0; loop < 1000 and state.step.load() < 1; ++loop)
            {
                Thread::Sleep(1);
            }
            SC_TEST_EXPECT(state.step.load() == 1);

            Result secondStart = secondPool.start(scheduler, {secondWorkers, NumWorkers}, {secondThreads, NumWorkers});
            SC_TEST_EXPECT(not secondStart);
            SC_TEST_EXPECT(not secondPool.isRunning());
            for (FiberWorkerThread& thread : secondThreads)
            {
                SC_TEST_EXPECT(not thread.wasStarted());
            }

            SC_TEST_EXPECT(scheduler.done(counter));
            SC_TEST_EXPECT(firstPool.join());
            SC_TEST_EXPECT(state.step.load() == 2);
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }
    }

    void workerPoolTaskWaves()
    {
        static constexpr size_t NumWorkers = 4;
        static constexpr size_t NumTasks   = 8;
        static constexpr size_t NumWaves   = 8;
        static constexpr int    NumYields  = 3;

        struct State
        {
            Atomic<int32_t> completed;
            Atomic<int32_t> taskRuns[NumTasks];
        };

        FiberScheduler    scheduler;
        FiberTask         tasks[NumTasks];
        static char       stackMemory[NumTasks * 64 * 1024] = {};
        FiberTaskPool     taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;
        State             state;

        for (size_t wave = 0; wave < NumWaves; ++wave)
        {
            for (size_t idx = 0; idx < NumTasks; ++idx)
            {
                SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                             [&state, idx](FiberScheduler& scheduler)
                                                             {
                                                                 for (int loop = 0; loop < NumYields; ++loop)
                                                                 {
                                                                     SC_TRY(scheduler.yield());
                                                                 }
                                                                 state.taskRuns[idx].fetch_add(1);
                                                                 state.completed.fetch_add(1);
                                                                 return Result(true);
                                                             })));
            }

            SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>((wave + 1) * NumTasks));
            for (size_t idx = 0; idx < NumTasks; ++idx)
            {
                SC_TEST_EXPECT(tasks[idx].result());
                SC_TEST_EXPECT(state.taskRuns[idx].load() == static_cast<int32_t>(wave + 1));
            }
        }
    }

    void workerPoolBenchmark()
    {
        static constexpr size_t NumWorkers             = 4;
        static constexpr size_t NumTasks               = 256;
        static constexpr size_t StackSize              = 64 * 1024;
        static constexpr size_t DequeCapacityPerWorker = NumTasks;
        static constexpr int    NumYields              = 4096;

        struct State
        {
            Atomic<int32_t> completed;
            Atomic<int32_t> totalYields;
        };

        FiberScheduler    scheduler;
        FiberTask         tasks[NumTasks];
        static char       stackMemory[NumTasks * StackSize] = {};
        FiberTaskPool     taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, StackSize);
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;
        FiberAllocator    allocator;
        char              dequeStorage[64 * 1024] = {};
        State             state;

        FiberWorkerPoolOptions workerPoolOptions;
        workerPoolOptions.dequeAllocator         = &allocator;
        workerPoolOptions.dequeCapacityPerWorker = DequeCapacityPerWorker;

        SC_TEST_EXPECT(allocator.createFixed(dequeStorage));
        taskPool.fillHighWaterMarks();

        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                         [&state](FiberScheduler& scheduler)
                                                         {
                                                             for (int loop = 0; loop < NumYields; ++loop)
                                                             {
                                                                 state.totalYields.fetch_add(1);
                                                                 SC_TRY(scheduler.yield());
                                                             }
                                                             state.completed.fetch_add(1);
                                                             return Result(true);
                                                         })));
        }

        Time::HighResolutionCounter start;
        start.snap();
        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, workerPoolOptions));
        SC_TEST_EXPECT(workerPool.join());
        Time::HighResolutionCounter finish;
        finish.snap();
        SC_TEST_EXPECT(allocator.used() == 0);
        const FiberAllocatorStatistics allocatorStatistics = allocator.statistics();
        SC_TEST_EXPECT(allocator.close());

        const Time::HighResolutionCounter elapsed     = finish.subtractExact(start);
        const int64_t                     elapsedNs   = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
        const int64_t                     elapsedMs   = elapsed.toMilliseconds().ms;
        const int64_t                     completed   = state.completed.load();
        const int64_t                     totalYields = state.totalYields.load();
        const int64_t                     tasksPerSec = completed * 1000000000 / elapsedNs;
        const int64_t                     yieldsPerSec = totalYields * 1000000000 / elapsedNs;

        FiberWorkerDiagnostics diagnostics;
        scheduler.workerDiagnostics({workers, NumWorkers}, diagnostics);
        const size_t totalSteals = diagnostics.stolenFibers;

        FiberSchedulerDiagnostics schedulerDiagnostics;
        scheduler.schedulerDiagnostics(schedulerDiagnostics);

        size_t maxStackUsed = 0;
        for (size_t idx = 0; idx < taskPool.capacity(); ++idx)
        {
            size_t usedBytes = 0;
            SC_TEST_EXPECT(taskPool.stackHighWaterUsedBytes(idx, usedBytes));
            if (usedBytes > maxStackUsed)
            {
                maxStackUsed = usedBytes;
            }
        }

        SC_TEST_EXPECT(completed == static_cast<int64_t>(NumTasks));
        SC_TEST_EXPECT(totalYields == static_cast<int64_t>(NumTasks * NumYields));
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());

        report.console.print("Fibers worker pool benchmark: workers={} tasks={}\n", NumWorkers, NumTasks);
        report.console.print("  dequeCapacityPerWorker={}\n", DequeCapacityPerWorker);
        report.console.print("  yieldsPerTask={} totalYields={}\n", static_cast<size_t>(NumYields),
                             static_cast<size_t>(totalYields));
        report.console.print("  elapsedMs={} elapsedNs={}\n", static_cast<size_t>(elapsedMs),
                             static_cast<size_t>(elapsedNs));
        report.console.print("  tasksPerSec={} yieldsPerSec={}\n", static_cast<size_t>(tasksPerSec),
                             static_cast<size_t>(yieldsPerSec));
        report.console.print("  totalSteals={} failedSteals={}\n", totalSteals, diagnostics.failedSteals);
        report.console.print("  queuePeak={} spilled={}\n", diagnostics.readyPeakFibers, diagnostics.spilledFibers);
        report.console.print("  runAttempts={} idlePolls={}\n", diagnostics.runAttempts, diagnostics.idlePolls);
        report.console.print("  executedFibers={} completedFibers={}\n", diagnostics.executedFibers,
                             diagnostics.completedFibers);
        report.console.print("  yieldedFibers={} waitingFibers={}\n", diagnostics.yieldedFibers,
                             diagnostics.waitingFibers);
        report.console.print("  schedulerLockContentions={} schedulerLockSpinRetries={}\n",
                             schedulerDiagnostics.lockContentions, schedulerDiagnostics.lockSpinRetries);
        report.console.print("  schedulerLockPeakSpinRetries={}\n", schedulerDiagnostics.lockPeakSpinRetries);
        report.console.print("  allocatorPeakBytes={} allocatorFailures={}\n", allocatorStatistics.peakBytesInUse,
                             allocatorStatistics.numAllocationFailures);
        report.console.print("  maxStackUsed={}\n", maxStackUsed);
        for (size_t idx = 0; idx < NumWorkers; ++idx)
        {
            report.console.print("  worker[{}] steals={}\n", idx, scheduler.stolenFiberCount(workers[idx]));
        }
    }

    void workerPoolPrimitives()
    {
        static constexpr size_t NumWorkers    = 4;
        static constexpr size_t NumWaiters    = 4;
        static constexpr size_t NumMutexTasks = 8;
        static constexpr int    NumMutexLoops = 4;

        {
            struct State
            {
                FiberEvent*     event = nullptr;
                Atomic<int32_t> started;
                Atomic<int32_t> completed;
            };

            FiberScheduler    scheduler;
            FiberEvent        event;
            FiberTask         tasks[NumWaiters + 1];
            static char       stackMemory[(NumWaiters + 1) * 64 * 1024] = {};
            FiberTaskPool     taskPool({tasks, NumWaiters + 1}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread threads[NumWorkers];
            FiberWorkerPool   workerPool;
            State             state;
            state.event = &event;

            for (size_t idx = 0; idx < NumWaiters; ++idx)
            {
                SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                             [&state](FiberScheduler& scheduler)
                                                             {
                                                                 state.started.fetch_add(1);
                                                                 SC_TRY(state.event->wait(scheduler));
                                                                 state.completed.fetch_add(1);
                                                                 return Result(true);
                                                             })));
            }
            SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                         [&state](FiberScheduler& scheduler)
                                                         {
                                                             while (state.started.load() !=
                                                                    static_cast<int32_t>(NumWaiters))
                                                             {
                                                                 SC_TRY(scheduler.yield());
                                                             }
                                                             return state.event->signal(scheduler);
                                                         })));

            SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumWaiters));
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                FiberAutoResetEvent* event = nullptr;
                Atomic<int32_t>      started;
                Atomic<int32_t>      completed;
            };

            FiberScheduler      scheduler;
            FiberAutoResetEvent event;
            FiberTask           tasks[NumWaiters + 1];
            static char         stackMemory[(NumWaiters + 1) * 64 * 1024] = {};
            FiberTaskPool       taskPool({tasks, NumWaiters + 1}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
            FiberWorker         workers[NumWorkers];
            FiberWorkerThread   threads[NumWorkers];
            FiberWorkerPool     workerPool;
            State               state;
            state.event = &event;

            for (size_t idx = 0; idx < NumWaiters; ++idx)
            {
                SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                             [&state](FiberScheduler& scheduler)
                                                             {
                                                                 state.started.fetch_add(1);
                                                                 SC_TRY(state.event->wait(scheduler));
                                                                 state.completed.fetch_add(1);
                                                                 return Result(true);
                                                             })));
            }
            SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                         [&state](FiberScheduler& scheduler)
                                                         {
                                                             while (state.started.load() !=
                                                                    static_cast<int32_t>(NumWaiters))
                                                             {
                                                                 SC_TRY(scheduler.yield());
                                                             }
                                                             for (size_t idx = 0; idx < NumWaiters; ++idx)
                                                             {
                                                                 SC_TRY(state.event->signal(scheduler));
                                                                 SC_TRY(scheduler.yield());
                                                             }
                                                             return Result(true);
                                                         })));

            SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumWaiters));
            SC_TEST_EXPECT(not event.isSignaled());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                FiberSemaphore* semaphore = nullptr;
                Atomic<int32_t> started;
                Atomic<int32_t> completed;
            };

            FiberScheduler    scheduler;
            FiberSemaphore    semaphore;
            FiberTask         tasks[NumWaiters + 1];
            static char       stackMemory[(NumWaiters + 1) * 64 * 1024] = {};
            FiberTaskPool     taskPool({tasks, NumWaiters + 1}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread threads[NumWorkers];
            FiberWorkerPool   workerPool;
            State             state;
            state.semaphore = &semaphore;

            for (size_t idx = 0; idx < NumWaiters; ++idx)
            {
                SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                             [&state](FiberScheduler& scheduler)
                                                             {
                                                                 state.started.fetch_add(1);
                                                                 SC_TRY(state.semaphore->wait(scheduler));
                                                                 state.completed.fetch_add(1);
                                                                 return Result(true);
                                                             })));
            }
            SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                         [&state](FiberScheduler& scheduler)
                                                         {
                                                             while (state.started.load() !=
                                                                    static_cast<int32_t>(NumWaiters))
                                                             {
                                                                 SC_TRY(scheduler.yield());
                                                             }
                                                             return state.semaphore->signal(scheduler, NumWaiters);
                                                         })));

            SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumWaiters));
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                FiberMutex* mutex  = nullptr;
                int         inside = 0;
                int         value  = 0;
            };

            FiberScheduler    scheduler;
            FiberMutex        mutex;
            FiberTask         tasks[NumMutexTasks];
            static char       stackMemory[NumMutexTasks * 64 * 1024] = {};
            FiberTaskPool     taskPool({tasks, NumMutexTasks}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread threads[NumWorkers];
            FiberWorkerPool   workerPool;
            State             state;
            state.mutex = &mutex;

            for (size_t idx = 0; idx < NumMutexTasks; ++idx)
            {
                SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                             [&state](FiberScheduler& scheduler)
                                                             {
                                                                 for (int loop = 0; loop < NumMutexLoops; ++loop)
                                                                 {
                                                                     SC_TRY(state.mutex->lock(scheduler));
                                                                     state.inside += 1;
                                                                     if (state.inside != 1)
                                                                     {
                                                                         return Result::Error(
                                                                             "FiberMutex allowed concurrent owners");
                                                                     }
                                                                     const int value = state.value;
                                                                     SC_TRY(scheduler.yield());
                                                                     state.value = value + 1;
                                                                     state.inside -= 1;
                                                                     SC_TRY(state.mutex->unlock(scheduler));
                                                                     SC_TRY(scheduler.yield());
                                                                 }
                                                                 return Result(true);
                                                             })));
            }

            SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(state.value == static_cast<int>(NumMutexTasks * NumMutexLoops));
            SC_TEST_EXPECT(state.inside == 0);
            SC_TEST_EXPECT(not mutex.isLocked());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            for (FiberTask& task : tasks)
            {
                SC_TEST_EXPECT(task.result());
            }
        }
    }

    void workerPoolCounterCancellationStress()
    {
        static constexpr size_t NumWorkers    = 4;
        static constexpr size_t NumTasks      = 64;
        static constexpr size_t StackSize     = FiberStackSize::ThirtyTwoKiB;
        static constexpr size_t NumYields     = 8;
        static constexpr size_t NumRounds     = 3;
        static constexpr size_t DequeCapacity = NumTasks;

        struct State
        {
            FiberCounter*   gate      = nullptr;
            Atomic<int32_t> entered   = 0;
            Atomic<int32_t> completed = 0;
            Atomic<int32_t> cancelled = 0;
        };

        auto runRound = [](bool cancelWaiters) -> Result
        {
            static FiberTask tasks[NumTasks];
            static char      stackMemory[NumTasks * StackSize] = {};
            static char      allocatorStorage[32 * 1024]       = {};

            FiberScheduler               scheduler;
            FiberCancellationTokenSource cancellationSource;
            FiberCounter                 gate;
            FiberTaskPool                taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, StackSize);
            FiberWorker                  workers[NumWorkers];
            FiberWorkerThread            threads[NumWorkers];
            FiberWorkerPool              workerPool;
            FiberAllocator               allocator;
            FiberWorkerPoolOptions       options;
            State                        state;
            state.gate = &gate;

            SC_TRY(allocator.createFixed(allocatorStorage));
            options.dequeAllocator         = &allocator;
            options.dequeCapacityPerWorker = DequeCapacity;

            scheduler.add(gate);
            FiberTaskSpawnOptions spawnOptions;
            if (cancelWaiters)
            {
                spawnOptions.cancellationToken = cancellationSource.token();
            }
            for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
            {
                SC_TRY(taskPool.spawn(scheduler,
                                      FiberTask::Procedure(
                                          [&state](FiberScheduler& currentScheduler)
                                          {
                                              state.entered.fetch_add(1, memory_order_release);
                                              Result waitResult = currentScheduler.wait(*state.gate);
                                              if (not waitResult)
                                              {
                                                  state.cancelled.fetch_add(1, memory_order_relaxed);
                                                  return waitResult;
                                              }
                                              for (size_t yieldIndex = 0; yieldIndex < NumYields; ++yieldIndex)
                                              {
                                                  SC_TRY(currentScheduler.yield());
                                              }
                                              state.completed.fetch_add(1, memory_order_relaxed);
                                              return Result(true);
                                          }),
                                      spawnOptions));
            }

            SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, options));

            size_t waitIterations = 0;
            while (state.entered.load(memory_order_acquire) != static_cast<int32_t>(NumTasks) and waitIterations < 1000)
            {
                Thread::Sleep(1);
                waitIterations += 1;
            }

            const bool allWaitersEntered = state.entered.load(memory_order_acquire) == static_cast<int32_t>(NumTasks);
            Result     releaseResult     = Result(true);
            if (allWaitersEntered)
            {
                if (cancelWaiters)
                {
                    releaseResult = scheduler.requestCancel(cancellationSource);
                }
                else
                {
                    releaseResult = scheduler.done(gate);
                }
            }
            else
            {
                releaseResult = scheduler.requestCancelAll();
            }

            SC_TRY(workerPool.join());
            SC_TRY(releaseResult);
            SC_TRY_MSG(allWaitersEntered, "Worker stress test did not enter every wait");
            SC_TRY_MSG(not scheduler.hasActiveFibers(), "Worker stress test left active fibers");
            SC_TRY_MSG(taskPool.availableCount() == NumTasks, "Worker stress test did not recycle every task slot");
            if (cancelWaiters)
            {
                SC_TRY_MSG(state.completed.load(memory_order_relaxed) == 0,
                           "Cancelled worker stress tasks completed normally");
                SC_TRY_MSG(state.cancelled.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks),
                           "Worker stress test did not cancel every waiter");
            }
            else
            {
                SC_TRY_MSG(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks),
                           "Worker stress test did not complete every task");
                SC_TRY_MSG(state.cancelled.load(memory_order_relaxed) == 0,
                           "Worker stress test unexpectedly cancelled a task");
            }
            return allocator.close();
        };

        for (size_t round = 0; round < NumRounds; ++round)
        {
            SC_TEST_EXPECT(runRound(false));
            SC_TEST_EXPECT(runRound(true));
        }
    }

    void workerPoolExternalSpawnStress()
    {
        static constexpr size_t NumWorkers    = 4;
        static constexpr size_t NumTasks      = 64;
        static constexpr size_t StackSize     = FiberStackSize::ThirtyTwoKiB;
        static constexpr size_t NumYields     = 4;
        static constexpr size_t NumRounds     = 3;
        static constexpr size_t DequeCapacity = NumTasks;

        struct State
        {
            FiberCounter*   gate      = nullptr;
            Atomic<int32_t> entered   = 0;
            Atomic<int32_t> completed = 0;
            Atomic<int32_t> cancelled = 0;
        };

        auto runRound = [](bool cancelWaiters) -> Result
        {
            static FiberTask anchor;
            static FiberTask tasks[NumTasks];
            static char      anchorStackMemory[StackSize]          = {};
            static char      taskStackMemory[NumTasks * StackSize] = {};
            static char      allocatorStorage[32 * 1024]           = {};

            FiberScheduler               scheduler;
            FiberCancellationTokenSource cancellationSource;
            FiberCounter                 gate;
            FiberWorker                  workers[NumWorkers];
            FiberWorkerThread            threads[NumWorkers];
            FiberWorkerPool              workerPool;
            FiberAllocator               allocator;
            FiberWorkerPoolOptions       workerPoolOptions;
            State                        state;
            state.gate = &gate;

            SC_TRY(allocator.createFixed(allocatorStorage));
            workerPoolOptions.dequeAllocator         = &allocator;
            workerPoolOptions.dequeCapacityPerWorker = DequeCapacity;
            workerPoolOptions.injectionAllocator     = &allocator;
            workerPoolOptions.injectionCapacity      = NumTasks;
            workerPoolOptions.idleSpinAttempts       = 0;

            scheduler.add(gate);
            FiberStack anchorStack({anchorStackMemory, sizeof(anchorStackMemory)});
            SC_TRY(scheduler.spawn(anchor, anchorStack,
                                   FiberTask::Procedure([&gate](FiberScheduler& currentScheduler)
                                                        { return currentScheduler.wait(gate); })));
            SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, workerPoolOptions));

            FiberTaskSpawnOptions spawnOptions;
            if (cancelWaiters)
            {
                spawnOptions.cancellationToken = cancellationSource.token();
            }
            for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
            {
                FiberStack taskStack({taskStackMemory + taskIndex * StackSize, StackSize});
                SC_TRY(scheduler.spawn(tasks[taskIndex], taskStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& currentScheduler)
                                           {
                                               state.entered.fetch_add(1, memory_order_release);
                                               Result waitResult = currentScheduler.wait(*state.gate);
                                               if (not waitResult)
                                               {
                                                   state.cancelled.fetch_add(1, memory_order_relaxed);
                                                   return waitResult;
                                               }
                                               for (size_t yieldIndex = 0; yieldIndex < NumYields; ++yieldIndex)
                                               {
                                                   SC_TRY(currentScheduler.yield());
                                               }
                                               state.completed.fetch_add(1, memory_order_relaxed);
                                               return Result(true);
                                           }),
                                       spawnOptions));
            }

            size_t waitIterations = 0;
            while (state.entered.load(memory_order_acquire) != static_cast<int32_t>(NumTasks) and waitIterations < 1000)
            {
                Thread::Sleep(1);
                waitIterations += 1;
            }

            const bool allWaitersEntered  = state.entered.load(memory_order_acquire) == static_cast<int32_t>(NumTasks);
            Result     cancellationResult = Result(true);
            if (cancelWaiters)
            {
                cancellationResult = scheduler.requestCancel(cancellationSource);
            }
            else if (not allWaitersEntered)
            {
                cancellationResult = scheduler.requestCancelAll();
            }

            size_t cancellationWaitIterations = 0;
            if (cancelWaiters)
            {
                while (state.cancelled.load(memory_order_acquire) != static_cast<int32_t>(NumTasks) and
                       cancellationWaitIterations < 1000)
                {
                    Thread::Sleep(1);
                    cancellationWaitIterations += 1;
                }
            }
            const Result releaseResult = scheduler.done(gate);

            SC_TRY(workerPool.join());
            FiberWorkerDiagnostics workerDiagnostics;
            scheduler.workerDiagnostics({workers, NumWorkers}, workerDiagnostics);
            SC_TRY(cancellationResult);
            SC_TRY(releaseResult);
            SC_TRY_MSG(allWaitersEntered, "External spawn stress test did not enter every wait");
            SC_TRY_MSG(anchor.result(), "External spawn stress anchor did not complete");
            SC_TRY_MSG(not scheduler.hasActiveFibers(), "External spawn stress test left active fibers");
            SC_TRY_MSG(workerDiagnostics.idleSpinIterations == 0,
                       "External spawn stress test ignored the immediate-parking option");
            SC_TRY_MSG(workerDiagnostics.parkAttempts > 0,
                       "External spawn stress test did not park with immediate parking enabled");
            if (cancelWaiters)
            {
                SC_TRY_MSG(cancellationWaitIterations < 1000,
                           "External spawn stress test did not observe every cancellation before releasing its anchor");
                SC_TRY_MSG(state.completed.load(memory_order_relaxed) == 0,
                           "Cancelled external spawn stress tasks completed normally");
                SC_TRY_MSG(state.cancelled.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks),
                           "External spawn stress test did not cancel every task");
            }
            else
            {
                SC_TRY_MSG(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks),
                           "External spawn stress test did not complete every task");
                SC_TRY_MSG(state.cancelled.load(memory_order_relaxed) == 0,
                           "External spawn stress test unexpectedly cancelled a task");
            }
            for (FiberTask& task : tasks)
            {
                SC_TRY_MSG(cancelWaiters ? not task.result() : task.result(),
                           "External spawn stress task result did not match the round");
            }
            return allocator.close();
        };

        for (size_t round = 0; round < NumRounds; ++round)
        {
            SC_TEST_EXPECT(runRound(false));
            SC_TEST_EXPECT(runRound(true));
        }
    }

    void workerPoolConcurrentExternalStress()
    {
        static constexpr size_t NumWorkers          = 4;
        static constexpr size_t NumProducers        = 4;
        static constexpr size_t NumTasksPerProducer = 16;
        static constexpr size_t NumTasks            = NumProducers * NumTasksPerProducer;
        static constexpr size_t NumCancelledTasks   = NumTasks / 2;
        static constexpr size_t StackSize           = FiberStackSize::ThirtyTwoKiB;
        static constexpr size_t NumYields           = 4;
        static constexpr size_t NumRounds           = 3;
        static constexpr size_t DequeCapacity       = NumTasks;

        struct State
        {
            FiberCounter*   gate      = nullptr;
            Atomic<int32_t> entered   = 0;
            Atomic<int32_t> completed = 0;
            Atomic<int32_t> cancelled = 0;
        };

        struct Producer
        {
            FiberScheduler*        scheduler = nullptr;
            FiberCancellationToken cancellationToken;
            Atomic<bool>*          mayStart    = nullptr;
            State*                 state       = nullptr;
            FiberTask*             tasks       = nullptr;
            char*                  stackMemory = nullptr;
            Result                 result      = Result(true);
            bool                   cancelTasks = false;
        };

        auto runRound = []() -> Result
        {
            static FiberTask tasks[NumTasks];
            static char      stackMemory[NumTasks * StackSize] = {};
            static char      anchorStackMemory[StackSize]      = {};
            static char      allocatorStorage[32 * 1024]       = {};

            FiberScheduler               scheduler;
            FiberCancellationTokenSource cancellationSource;
            FiberCounter                 gate;
            FiberTask                    anchor;
            FiberWorker                  workers[NumWorkers];
            FiberWorkerThread            workerThreads[NumWorkers];
            FiberWorkerPool              workerPool;
            FiberAllocator               allocator;
            FiberWorkerPoolOptions       workerPoolOptions;
            Thread                       producerThreads[NumProducers];
            Producer                     producers[NumProducers];
            Atomic<bool>                 producersMayStart = false;
            State                        state;
            state.gate = &gate;

            SC_TRY(allocator.createFixed(allocatorStorage));
            workerPoolOptions.dequeAllocator         = &allocator;
            workerPoolOptions.dequeCapacityPerWorker = DequeCapacity;
            workerPoolOptions.injectionAllocator     = &allocator;
            workerPoolOptions.injectionCapacity      = NumTasks;

            scheduler.add(gate);
            FiberStack anchorStack({anchorStackMemory, sizeof(anchorStackMemory)});
            SC_TRY(scheduler.spawn(anchor, anchorStack,
                                   FiberTask::Procedure([&gate](FiberScheduler& currentScheduler)
                                                        { return currentScheduler.wait(gate); })));
            SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {workerThreads, NumWorkers}, workerPoolOptions));

            for (size_t producerIndex = 0; producerIndex < NumProducers; ++producerIndex)
            {
                Producer& producer         = producers[producerIndex];
                producer.scheduler         = &scheduler;
                producer.cancellationToken = cancellationSource.token();
                producer.mayStart          = &producersMayStart;
                producer.state             = &state;
                producer.tasks             = tasks + producerIndex * NumTasksPerProducer;
                producer.stackMemory       = stackMemory + producerIndex * NumTasksPerProducer * StackSize;
                producer.cancelTasks       = producerIndex % 2 != 0;

                SC_TRY(producerThreads[producerIndex].start(
                    [&producer](Thread&)
                    {
                        while (not producer.mayStart->load(memory_order_acquire))
                        {
                            Thread::Sleep(1);
                        }

                        FiberTaskSpawnOptions spawnOptions;
                        if (producer.cancelTasks)
                        {
                            spawnOptions.cancellationToken = producer.cancellationToken;
                        }
                        for (size_t taskIndex = 0; taskIndex < NumTasksPerProducer; ++taskIndex)
                        {
                            FiberStack taskStack({producer.stackMemory + taskIndex * StackSize, StackSize});
                            producer.result = producer.scheduler->spawn(
                                producer.tasks[taskIndex], taskStack,
                                FiberTask::Procedure(
                                    [state = producer.state](FiberScheduler& currentScheduler)
                                    {
                                        state->entered.fetch_add(1, memory_order_release);
                                        Result waitResult = currentScheduler.wait(*state->gate);
                                        if (not waitResult)
                                        {
                                            state->cancelled.fetch_add(1, memory_order_relaxed);
                                            return waitResult;
                                        }
                                        for (size_t yieldIndex = 0; yieldIndex < NumYields; ++yieldIndex)
                                        {
                                            SC_TRY(currentScheduler.yield());
                                        }
                                        state->completed.fetch_add(1, memory_order_relaxed);
                                        return Result(true);
                                    }),
                                spawnOptions);
                            if (not producer.result)
                            {
                                return;
                            }
                        }
                    }));
            }

            producersMayStart.store(true, memory_order_release);
            bool producerFailed = false;
            for (size_t producerIndex = 0; producerIndex < NumProducers; ++producerIndex)
            {
                SC_TRY(producerThreads[producerIndex].join());
                producerFailed = producerFailed or not producers[producerIndex].result;
            }
            if (producerFailed)
            {
                SC_TRY(scheduler.requestCancelAll());
                SC_TRY(workerPool.join());
                return Result::Error("Concurrent external stress producer failed");
            }

            size_t waitIterations = 0;
            while (state.entered.load(memory_order_acquire) != static_cast<int32_t>(NumTasks) and waitIterations < 1000)
            {
                Thread::Sleep(1);
                waitIterations += 1;
            }
            if (state.entered.load(memory_order_acquire) != static_cast<int32_t>(NumTasks))
            {
                SC_TRY(scheduler.requestCancelAll());
                SC_TRY(workerPool.join());
                return Result::Error("Concurrent external stress tasks did not enter their waits");
            }

            SC_TRY(scheduler.requestCancel(cancellationSource));

            size_t cancellationWaitIterations = 0;
            while (state.cancelled.load(memory_order_acquire) != static_cast<int32_t>(NumCancelledTasks) and
                   cancellationWaitIterations < 1000)
            {
                Thread::Sleep(1);
                cancellationWaitIterations += 1;
            }
            if (state.cancelled.load(memory_order_acquire) != static_cast<int32_t>(NumCancelledTasks))
            {
                SC_TRY(scheduler.requestCancelAll());
                SC_TRY(workerPool.join());
                return Result::Error("Concurrent external stress tasks did not observe cancellation");
            }

            SC_TRY(scheduler.done(gate));
            SC_TRY(workerPool.join());

            FiberWorkerDiagnostics workerDiagnostics;
            scheduler.workerDiagnostics({workers, NumWorkers}, workerDiagnostics);
            SC_TRY_MSG(anchor.result(), "Concurrent external stress anchor did not complete");
            SC_TRY_MSG(not scheduler.hasActiveFibers(), "Concurrent external stress left active fibers");
            SC_TRY_MSG(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks - NumCancelledTasks),
                       "Concurrent external stress did not complete every non-cancelled task");
            SC_TRY_MSG(state.cancelled.load(memory_order_relaxed) == static_cast<int32_t>(NumCancelledTasks),
                       "Concurrent external stress did not cancel every selected task");
            SC_TRY_MSG(workerDiagnostics.executedFibers >= NumTasks + 1,
                       "Concurrent external stress did not execute every externally submitted task");
            for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
            {
                const bool shouldBeCancelled = (taskIndex / NumTasksPerProducer) % 2 != 0;
                SC_TRY_MSG(shouldBeCancelled ? not tasks[taskIndex].result() : tasks[taskIndex].result(),
                           "Concurrent external stress task result did not match its producer policy");
            }
            return allocator.close();
        };

        for (size_t round = 0; round < NumRounds; ++round)
        {
            SC_TEST_EXPECT(runRound());
        }
    }

    void workerPoolMixedTransitionStress()
    {
        static constexpr size_t NumWorkers    = 4;
        static constexpr size_t NumTasks      = 64;
        static constexpr size_t NumRounds     = 5;
        static constexpr size_t StackSize     = FiberStackSize::ThirtyTwoKiB;
        static constexpr size_t DequeCapacity = NumTasks;

        struct State
        {
            Atomic<bool>    allowFinish;
            Atomic<int32_t> completed;
            Atomic<int32_t> cancelled;
            Atomic<int32_t> deterministicSteps;
        };
        struct Context
        {
            State*        state     = nullptr;
            FiberCounter* counter   = nullptr;
            size_t        numYields = 0;
        };
        struct ReleaseContext
        {
            FiberTask*    tasks        = nullptr;
            FiberCounter* counters     = nullptr;
            size_t*       publishOrder = nullptr;
            bool*         cancelTask   = nullptr;
        };

        auto runRound = [](uint32_t seed) -> Result
        {
            static FiberTask tasks[NumTasks];
            static char      stackMemory[NumTasks * StackSize] = {};
            static char      allocatorStorage[16 * 1024]       = {};
            static FiberTask releaseTask;
            static char      releaseStackMemory[StackSize] = {};

            FiberScheduler    scheduler;
            FiberCounter      counters[NumTasks];
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread workerThreads[NumWorkers];
            FiberWorkerPool   workerPool;
            FiberAllocator    allocator;
            Context           contexts[NumTasks];
            size_t            publishOrder[NumTasks];
            bool              cancelTask[NumTasks] = {};
            State             state;

            SC_TRY(allocator.createFixed(allocatorStorage));
            SC_TRY(scheduler.createWorkerDeques(allocator, {workers, NumWorkers}, DequeCapacity));

            size_t expectedCompleted = 0;
            size_t expectedCancelled = 0;
            size_t expectedSteps     = 0;
            for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
            {
                seed                          = seed * 1664525u + 1013904223u;
                cancelTask[taskIndex]         = (seed & 3u) == 0;
                contexts[taskIndex].state     = &state;
                contexts[taskIndex].counter   = &counters[taskIndex];
                contexts[taskIndex].numYields = 1 + ((seed >> 8u) & 7u);
                publishOrder[taskIndex]       = taskIndex;
                expectedCancelled += cancelTask[taskIndex] ? 1 : 0;
                expectedCompleted += cancelTask[taskIndex] ? 0 : 1;
                expectedSteps += cancelTask[taskIndex] ? 0 : contexts[taskIndex].numYields;

                scheduler.add(counters[taskIndex]);
                Context*   context = &contexts[taskIndex];
                FiberStack stack({stackMemory + taskIndex * StackSize, StackSize});
                SC_TRY(
                    scheduler.spawn(tasks[taskIndex], stack,
                                    FiberTask::Procedure(
                                        [context](FiberScheduler& currentScheduler)
                                        {
                                            Result waitResult = currentScheduler.wait(*context->counter);
                                            if (not waitResult)
                                            {
                                                context->state->cancelled.fetch_add(1, memory_order_relaxed);
                                                return waitResult;
                                            }
                                            while (not context->state->allowFinish.load(memory_order_acquire))
                                            {
                                                SC_TRY(currentScheduler.yield());
                                            }
                                            for (size_t yieldIndex = 0; yieldIndex < context->numYields; ++yieldIndex)
                                            {
                                                context->state->deterministicSteps.fetch_add(1, memory_order_relaxed);
                                                SC_TRY(currentScheduler.yield());
                                            }
                                            context->state->completed.fetch_add(1, memory_order_relaxed);
                                            return Result(true);
                                        })));
                SC_TRY(scheduler.runNoWait(workers[0], {workers, NumWorkers}));
                SC_TRY_MSG(tasks[taskIndex].status() == FiberTaskStatus::Waiting,
                           "Mixed transition stress task did not reach its wait");
            }

            for (size_t index = NumTasks; index > 1; --index)
            {
                seed                    = seed * 1664525u + 1013904223u;
                const size_t swapIdx    = seed % index;
                const size_t current    = publishOrder[index - 1];
                publishOrder[index - 1] = publishOrder[swapIdx];
                publishOrder[swapIdx]   = current;
            }

            ReleaseContext releaseContext;
            releaseContext.tasks        = tasks;
            releaseContext.counters     = counters;
            releaseContext.publishOrder = publishOrder;
            releaseContext.cancelTask   = cancelTask;
            ReleaseContext* release     = &releaseContext;
            FiberStack      releaseStack({releaseStackMemory, sizeof(releaseStackMemory)});
            SC_TRY(scheduler.spawn(releaseTask, releaseStack,
                                   FiberTask::Procedure(
                                       [release](FiberScheduler& currentScheduler)
                                       {
                                           for (size_t publishIndex = 0; publishIndex < NumTasks; ++publishIndex)
                                           {
                                               const size_t taskIndex = release->publishOrder[publishIndex];
                                               if (release->cancelTask[taskIndex])
                                               {
                                                   SC_TRY(currentScheduler.requestCancel(release->tasks[taskIndex]));
                                               }
                                               SC_TRY(currentScheduler.done(release->counters[taskIndex]));
                                           }
                                           return Result(true);
                                       })));
            SC_TRY(scheduler.runNoWait(workers[0], {workers, NumWorkers}));
            SC_TRY(releaseTask.result());
            SC_TRY_MSG(scheduler.readyFiberCount(workers[0]) == NumTasks,
                       "Mixed transition stress did not prepare the owner backlog");

            SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {workerThreads, NumWorkers}));
            bool observedSteal = false;
            for (size_t attempt = 0; attempt < 1000; ++attempt)
            {
                FiberWorkerDiagnostics activeDiagnostics;
                scheduler.workerDiagnostics({workers, NumWorkers}, activeDiagnostics);
                if (activeDiagnostics.stolenFibers > 0)
                {
                    observedSteal = true;
                    break;
                }
                Thread::Sleep(1);
            }
            state.allowFinish.store(true, memory_order_release);
            SC_TRY(workerPool.join());

            FiberWorkerDiagnostics diagnostics;
            scheduler.workerDiagnostics({workers, NumWorkers}, diagnostics);
            scheduler.releaseWorkerDeques({workers, NumWorkers});
            SC_TRY(allocator.close());
            SC_TRY_MSG(not scheduler.hasActiveFibers(), "Mixed transition stress left active fibers");
            SC_TRY_MSG(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(expectedCompleted),
                       "Mixed transition stress completed count mismatch");
            SC_TRY_MSG(state.cancelled.load(memory_order_relaxed) == static_cast<int32_t>(expectedCancelled),
                       "Mixed transition stress cancellation count mismatch");
            SC_TRY_MSG(state.deterministicSteps.load(memory_order_relaxed) == static_cast<int32_t>(expectedSteps),
                       "Mixed transition stress lost or duplicated a yielded continuation");
            SC_TRY_MSG(observedSteal and diagnostics.stolenFibers > 0,
                       "Mixed transition stress did not steal owner work");
            for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
            {
                SC_TRY_MSG(cancelTask[taskIndex] ? not tasks[taskIndex].result() : tasks[taskIndex].result(),
                           "Mixed transition stress task result mismatch");
                SC_TRY_MSG(counters[taskIndex].value() == 0, "Mixed transition stress counter did not drain");
            }
            return Result(true);
        };

        uint32_t seed = 0xC001CAFEu;
        for (size_t round = 0; round < NumRounds; ++round)
        {
            SC_TEST_EXPECT(runRound(seed));
            seed = seed * 1664525u + 1013904223u;
        }
    }

    void workerStealing()
    {
        {
            struct State
            {
                int  step          = 0;
                bool ranAfterSteal = false;
            };

            FiberScheduler scheduler;
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});
            FiberWorker    workers[2];
            State          state;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   state.step = 1;
                                                   SC_TRY(scheduler.yield());
                                                   state.step          = 2;
                                                   state.ranAfterSteal = true;
                                                   return Result(true);
                                               })));

            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(state.step == 1);
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 1);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 1);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[1]) == 0);
            SC_TEST_EXPECT(scheduler.stolenFiberCount(workers[0]) == 0);
            SC_TEST_EXPECT(scheduler.stolenFiberCount(workers[1]) == 0);
            SC_TEST_EXPECT(scheduler.stolenFiberCount({workers, 2}) == 0);
            SC_TEST_EXPECT(not task.isCompleted());

            FiberWorker unrelatedWorker;
            Result      unrelatedRun = scheduler.runNoWait(unrelatedWorker);
            SC_TEST_EXPECT(not unrelatedRun);
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 1);
            SC_TEST_EXPECT(scheduler.readyFiberCount(unrelatedWorker) == 0);
            SC_TEST_EXPECT(scheduler.stolenFiberCount(unrelatedWorker) == 0);

            SC_TEST_EXPECT(scheduler.runReadyFibers(workers[1], {workers, 2}));
            SC_TEST_EXPECT(state.step == 2);
            SC_TEST_EXPECT(state.ranAfterSteal);
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(task.result());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 0);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 0);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[1]) == 0);
            SC_TEST_EXPECT(scheduler.stolenFiberCount(workers[1]) == 1);
            SC_TEST_EXPECT(scheduler.stolenFiberCount({workers, 2}) == 1);
            scheduler.resetWorkerDiagnostics(workers[1]);
            SC_TEST_EXPECT(scheduler.stolenFiberCount({workers, 2}) == 0);
        }

        {
            struct State
            {
                int taskSteps[2] = {};
                int firstResumed = -1;
            };

            FiberScheduler scheduler;
            FiberTask      tasks[2];
            char           stackMemory[2][64 * 1024] = {};
            FiberStack     stacks[2]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
            };
            FiberWorker workers[3];
            State       state;

            SC_TEST_EXPECT(scheduler.spawn(tasks[0], stacks[0],
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   state.taskSteps[0] = 1;
                                                   SC_TRY(scheduler.yield());
                                                   state.taskSteps[0] = 2;
                                                   if (state.firstResumed < 0)
                                                   {
                                                       state.firstResumed = 0;
                                                   }
                                                   return Result(true);
                                               })));
            SC_TEST_EXPECT(scheduler.spawn(tasks[1], stacks[1],
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   state.taskSteps[1] = 1;
                                                   SC_TRY(scheduler.yield());
                                                   state.taskSteps[1] = 2;
                                                   if (state.firstResumed < 0)
                                                   {
                                                       state.firstResumed = 2;
                                                   }
                                                   return Result(true);
                                               })));

            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 3}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[2], {workers, 3}));
            SC_TEST_EXPECT(state.taskSteps[0] == 1);
            SC_TEST_EXPECT(state.taskSteps[1] == 1);
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 2);

            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 3}));
            SC_TEST_EXPECT(state.firstResumed == 2);
            SC_TEST_EXPECT(tasks[1].isCompleted());
            SC_TEST_EXPECT(scheduler.runReadyFibers(workers[1], {workers, 3}));
            SC_TEST_EXPECT(tasks[0].isCompleted());
            SC_TEST_EXPECT(tasks[0].result());
            SC_TEST_EXPECT(tasks[1].result());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                FiberEvent* event       = nullptr;
                int         resumedTask = -1;
            };

            FiberScheduler scheduler;
            FiberEvent     event;
            FiberTask      tasks[3];
            char           stackMemory[3][64 * 1024] = {};
            FiberStack     stacks[3]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
            };
            FiberWorker workers[3];
            State       state;
            state.event = &event;

            for (int idx = 0; idx < 3; ++idx)
            {
                SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                               FiberTask::Procedure(
                                                   [&state, idx](FiberScheduler& scheduler)
                                                   {
                                                       SC_TRY(state.event->wait(scheduler));
                                                       if (state.resumedTask < 0)
                                                       {
                                                           state.resumedTask = idx;
                                                       }
                                                       return Result(true);
                                                   })));
            }

            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 3}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 3}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[2], {workers, 3}));
            SC_TEST_EXPECT(event.signal(scheduler));
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 2);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[2]) == 1);

            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 3}));
            SC_TEST_EXPECT(state.resumedTask == 0);
            SC_TEST_EXPECT(scheduler.stolenFiberCount(workers[1]) == 1);
            SC_TEST_EXPECT(scheduler.runReadyFibers(workers[1], {workers, 3}));
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            for (FiberTask& task : tasks)
            {
                SC_TEST_EXPECT(task.result());
            }
            scheduler.resetWorkerDiagnostics({workers, 3});
            SC_TEST_EXPECT(scheduler.stolenFiberCount({workers, 3}) == 0);
        }

        {
            struct State
            {
                FiberEvent* event       = nullptr;
                int         resumeOrder = -1;
            };

            FiberScheduler scheduler;
            FiberEvent     event;
            FiberTask      tasks[2];
            char           stackMemory[2][64 * 1024] = {};
            FiberStack     stacks[2]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
            };
            FiberWorker workers[2];
            State       state;
            state.event = &event;

            SC_TEST_EXPECT(scheduler.spawn(tasks[0], stacks[0],
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(state.event->wait(scheduler));
                                                   if (state.resumeOrder < 0)
                                                   {
                                                       state.resumeOrder = 0;
                                                   }
                                                   return Result(true);
                                               })));
            SC_TEST_EXPECT(scheduler.spawn(tasks[1], stacks[1],
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(state.event->wait(scheduler));
                                                   if (state.resumeOrder < 0)
                                                   {
                                                       state.resumeOrder = 1;
                                                   }
                                                   return Result(true);
                                               })));

            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(event.signal(scheduler));
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 2);

            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(state.resumeOrder == 1);
            SC_TEST_EXPECT(scheduler.runReadyFibers(workers[0], {workers, 2}));
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                FiberEvent* event       = nullptr;
                int         resumeOrder = -1;
            };

            FiberScheduler scheduler;
            FiberEvent     event;
            FiberTask      tasks[2];
            char           stackMemory[2][64 * 1024] = {};
            FiberStack     stacks[2]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
            };
            FiberWorker workers[2];
            State       state;
            state.event = &event;

            SC_TEST_EXPECT(scheduler.spawn(tasks[0], stacks[0],
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(state.event->wait(scheduler));
                                                   if (state.resumeOrder < 0)
                                                   {
                                                       state.resumeOrder = 0;
                                                   }
                                                   return Result(true);
                                               })));
            SC_TEST_EXPECT(scheduler.spawn(tasks[1], stacks[1],
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(state.event->wait(scheduler));
                                                   if (state.resumeOrder < 0)
                                                   {
                                                       state.resumeOrder = 1;
                                                   }
                                                   return Result(true);
                                               })));

            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(event.signal(scheduler));
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 2);

            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(state.resumeOrder == 0);
            SC_TEST_EXPECT(scheduler.runReadyFibers(workers[0], {workers, 2}));
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }
    }

    void workerDeque()
    {
        struct State
        {
            int resumeOrder[3] = {};
            int numResumed     = 0;
        };

        struct WaitContext
        {
            FiberCounter* counter = nullptr;
            State*        state   = nullptr;
            int           id      = 0;
        };

        auto spawnWaiters = [](FiberScheduler& scheduler, FiberCounter& counter, State& state, Span<FiberTask> tasks,
                               Span<FiberStack> stacks, Span<WaitContext> contexts) -> Result
        {
            scheduler.add(counter);
            for (size_t idx = 0; idx < tasks.sizeInElements(); ++idx)
            {
                contexts[idx].counter = &counter;
                contexts[idx].state   = &state;
                contexts[idx].id      = static_cast<int>(idx);
                WaitContext* context  = &contexts[idx];
                SC_TRY(scheduler.spawn(tasks[idx], stacks[idx],
                                       FiberTask::Procedure(
                                           [context](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.wait(*context->counter));
                                               context->state->resumeOrder[context->state->numResumed++] = context->id;
                                               return Result(true);
                                           })));
            }
            return Result(true);
        };

        {
            FiberScheduler   scheduler;
            FiberAllocator   allocator;
            char             allocatorStorage[4096] = {};
            FiberWorker      workers[2];
            static FiberTask tasks[4];
            static char      stackMemory[4][64 * 1024] = {};
            FiberStack       stacks[4]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
                FiberStack({stackMemory[3], sizeof(stackMemory[3])}),
            };
            WaitContext  contexts[3];
            FiberCounter counter;
            State        state;

            SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
            SC_TEST_EXPECT(scheduler.createWorkerDeques(allocator, {workers, 2}, 4));
            SC_TEST_EXPECT(spawnWaiters(scheduler, counter, state, {tasks, 3}, {stacks, 3}, {contexts, 3}));
            SC_TEST_EXPECT(scheduler.spawn(
                tasks[3], stacks[3],
                FiberTask::Procedure([&counter](FiberScheduler& scheduler) { return scheduler.done(counter); })));
            for (size_t idx = 0; idx < 3; ++idx)
            {
                SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            }
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 3);
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(state.numResumed == 3);
            SC_TEST_EXPECT(state.resumeOrder[0] == 2);
            SC_TEST_EXPECT(state.resumeOrder[1] == 1);
            SC_TEST_EXPECT(state.resumeOrder[2] == 0);
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            scheduler.releaseWorkerDeques({workers, 2});
            SC_TEST_EXPECT(allocator.close());
        }

        {
            FiberScheduler   scheduler;
            FiberAllocator   allocator;
            char             allocatorStorage[4096] = {};
            FiberWorker      workers[2];
            static FiberTask tasks[4];
            static char      stackMemory[4][64 * 1024] = {};
            FiberStack       stacks[4]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
                FiberStack({stackMemory[3], sizeof(stackMemory[3])}),
            };
            WaitContext  contexts[3];
            FiberCounter counter;
            State        state;

            SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
            SC_TEST_EXPECT(scheduler.createWorkerDeques(allocator, {workers, 2}, 4));
            SC_TEST_EXPECT(spawnWaiters(scheduler, counter, state, {tasks, 3}, {stacks, 3}, {contexts, 3}));
            SC_TEST_EXPECT(scheduler.spawn(
                tasks[3], stacks[3],
                FiberTask::Procedure([&counter](FiberScheduler& scheduler) { return scheduler.done(counter); })));
            for (size_t idx = 0; idx < 3; ++idx)
            {
                SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            }
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 3);
            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(state.numResumed == 3);
            SC_TEST_EXPECT(state.resumeOrder[0] == 0);
            SC_TEST_EXPECT(state.resumeOrder[1] == 1);
            SC_TEST_EXPECT(state.resumeOrder[2] == 2);
            SC_TEST_EXPECT(scheduler.stolenFiberCount(workers[1]) == 3);
            FiberWorkerDiagnostics thiefDiagnostics;
            scheduler.workerDiagnostics(workers[1], thiefDiagnostics);
            SC_TEST_EXPECT(thiefDiagnostics.stealAttempts == 1);
            SC_TEST_EXPECT(thiefDiagnostics.stealVictimProbes == 1);
            SC_TEST_EXPECT(thiefDiagnostics.stolenFibers == 3);
            SC_TEST_EXPECT(thiefDiagnostics.stolenBatches == 1);
            SC_TEST_EXPECT(thiefDiagnostics.stolenBatchPeak == 3);
            SC_TEST_EXPECT(thiefDiagnostics.failedSteals == 0);
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            scheduler.releaseWorkerDeques({workers, 2});
            SC_TEST_EXPECT(allocator.close());
        }

        {
            FiberScheduler   scheduler;
            FiberAllocator   allocator;
            char             allocatorStorage[4096] = {};
            FiberWorker      workers[2];
            static FiberTask tasks[4];
            static char      stackMemory[4][64 * 1024] = {};
            FiberStack       stacks[4]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
                FiberStack({stackMemory[3], sizeof(stackMemory[3])}),
            };
            WaitContext  contexts[3];
            FiberCounter counter;
            State        state;

            SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
            SC_TEST_EXPECT(scheduler.createWorkerDeques(allocator, {workers, 2}, 2));
            SC_TEST_EXPECT(spawnWaiters(scheduler, counter, state, {tasks, 3}, {stacks, 3}, {contexts, 3}));
            SC_TEST_EXPECT(scheduler.spawn(
                tasks[3], stacks[3],
                FiberTask::Procedure([&counter](FiberScheduler& scheduler) { return scheduler.done(counter); })));
            for (size_t idx = 0; idx < 3; ++idx)
            {
                SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            }
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 3);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 2);
            FiberWorkerDiagnostics spillDiagnostics;
            scheduler.workerDiagnostics(workers[0], spillDiagnostics);
            SC_TEST_EXPECT(spillDiagnostics.readyFibers == 2);
            SC_TEST_EXPECT(spillDiagnostics.readyPeakFibers == 2);
            SC_TEST_EXPECT(spillDiagnostics.dequeCapacity == 2);
            SC_TEST_EXPECT(spillDiagnostics.spilledFibers == 1);
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(state.numResumed == 3);
            SC_TEST_EXPECT(state.resumeOrder[0] == 1);
            SC_TEST_EXPECT(state.resumeOrder[1] == 0);
            SC_TEST_EXPECT(state.resumeOrder[2] == 2);
            scheduler.workerDiagnostics(workers[0], spillDiagnostics);
            SC_TEST_EXPECT(spillDiagnostics.readyFibers == 0);
            SC_TEST_EXPECT(spillDiagnostics.readyPeakFibers == 2);
            SC_TEST_EXPECT(spillDiagnostics.spilledFibers == 1);
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            scheduler.releaseWorkerDeques({workers, 2});
            SC_TEST_EXPECT(allocator.close());
        }

        {
            FiberScheduler scheduler;
            FiberAllocator allocator;
            char           allocatorStorage[4096] = {};
            FiberWorker    workers[2];

            SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
            SC_TEST_EXPECT(scheduler.createWorkerDeques(allocator, {workers, 2}, 1));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));

            FiberWorkerDiagnostics diagnostics;
            scheduler.workerDiagnostics(workers[0], diagnostics);
            SC_TEST_EXPECT(diagnostics.stealAttempts == 1);
            SC_TEST_EXPECT(diagnostics.stealVictimProbes == 1);
            SC_TEST_EXPECT(diagnostics.stolenFibers == 0);
            SC_TEST_EXPECT(diagnostics.failedSteals == 1);

            scheduler.resetWorkerDiagnostics(workers[0]);
            scheduler.workerDiagnostics(workers[0], diagnostics);
            SC_TEST_EXPECT(diagnostics.readyPeakFibers == 0);
            SC_TEST_EXPECT(diagnostics.spilledFibers == 0);
            SC_TEST_EXPECT(diagnostics.stealAttempts == 0);
            SC_TEST_EXPECT(diagnostics.stealVictimProbes == 0);
            SC_TEST_EXPECT(diagnostics.stolenFibers == 0);
            SC_TEST_EXPECT(diagnostics.stolenBatches == 0);
            SC_TEST_EXPECT(diagnostics.stolenBatchPeak == 0);
            SC_TEST_EXPECT(diagnostics.failedSteals == 0);
            SC_TEST_EXPECT(diagnostics.runAttempts == 0);
            SC_TEST_EXPECT(diagnostics.idlePolls == 0);
            SC_TEST_EXPECT(diagnostics.executedFibers == 0);
            SC_TEST_EXPECT(diagnostics.completedFibers == 0);
            SC_TEST_EXPECT(diagnostics.yieldedFibers == 0);
            SC_TEST_EXPECT(diagnostics.waitingFibers == 0);
            scheduler.releaseWorkerDeques({workers, 2});
            SC_TEST_EXPECT(allocator.close());
        }

        {
            struct CancelState
            {
                int cancelled[2] = {};
            };

            struct CancelContext
            {
                FiberEvent*  event = nullptr;
                CancelState* state = nullptr;
                int          id    = 0;
            };

            FiberScheduler   scheduler;
            FiberAllocator   allocator;
            char             allocatorStorage[4096] = {};
            FiberWorker      workers[2];
            FiberEvent       event;
            static FiberTask tasks[2];
            static char      stackMemory[2][64 * 1024] = {};
            FiberStack       stacks[2]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
            };
            CancelContext contexts[2];
            CancelState   state;

            SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
            SC_TEST_EXPECT(scheduler.createWorkerDeques(allocator, {workers, 2}, 4));
            for (size_t idx = 0; idx < 2; ++idx)
            {
                contexts[idx].event    = &event;
                contexts[idx].state    = &state;
                contexts[idx].id       = static_cast<int>(idx);
                CancelContext* context = &contexts[idx];
                SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                               FiberTask::Procedure(
                                                   [context](FiberScheduler& scheduler)
                                                   {
                                                       Result result = context->event->wait(scheduler);
                                                       if (not result)
                                                       {
                                                           context->state->cancelled[context->id]++;
                                                       }
                                                       return result;
                                                   })));
                SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
                SC_TEST_EXPECT(tasks[idx].status() == FiberTaskStatus::Waiting);
            }

            SC_TEST_EXPECT(scheduler.requestCancel(tasks[0]));
            SC_TEST_EXPECT(scheduler.requestCancel(tasks[1]));
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 2);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 0);
            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(state.cancelled[0] == 1);
            SC_TEST_EXPECT(state.cancelled[1] == 1);
            SC_TEST_EXPECT(scheduler.stolenFiberCount(workers[1]) == 0);
            SC_TEST_EXPECT(not tasks[0].result());
            SC_TEST_EXPECT(not tasks[1].result());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            scheduler.releaseWorkerDeques({workers, 2});
            SC_TEST_EXPECT(allocator.close());
        }

        {
            FiberScheduler   scheduler;
            FiberAllocator   allocator;
            char             allocatorStorage[4096] = {};
            FiberWorker      workers[2];
            static FiberTask tasks[3];
            static char      stackMemory[3][64 * 1024] = {};
            FiberStack       stacks[3]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
            };
            WaitContext  contexts[3];
            FiberCounter counter;
            State        state;

            SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
            SC_TEST_EXPECT(scheduler.createWorkerDeques(allocator, {workers, 2}, 1));
            SC_TEST_EXPECT(spawnWaiters(scheduler, counter, state, {tasks, 3}, {stacks, 3}, {contexts, 3}));
            for (size_t idx = 0; idx < 3; ++idx)
            {
                SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            }
            SC_TEST_EXPECT(scheduler.done(counter));
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 3);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 0);
            SC_TEST_EXPECT(scheduler.shutdown(workers[1], {workers, 2}));
            SC_TEST_EXPECT(not tasks[0].result());
            SC_TEST_EXPECT(not tasks[1].result());
            SC_TEST_EXPECT(not tasks[2].result());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            scheduler.releaseWorkerDeques({workers, 2});
            SC_TEST_EXPECT(allocator.close());
        }

        {
            struct RaceContext
            {
                FiberCounter* counter = nullptr;
                int*          runs    = nullptr;
            };

            FiberScheduler   scheduler;
            FiberAllocator   allocator;
            char             allocatorStorage[4096] = {};
            FiberWorker      workers[2];
            static FiberTask tasks[3];
            static char      stackMemory[3][64 * 1024] = {};
            FiberStack       stacks[3]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
            };
            FiberCounter counters[2];
            RaceContext  contexts[2];
            int          waitRuns[2] = {};
            int          yieldRuns   = 0;

            SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
            SC_TEST_EXPECT(scheduler.createWorkerDeques(allocator, {workers, 2}, 4));

            for (size_t idx = 0; idx < 2; ++idx)
            {
                scheduler.add(counters[idx]);
                contexts[idx].counter = &counters[idx];
                contexts[idx].runs    = &waitRuns[idx];
                RaceContext* context  = &contexts[idx];
                SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                               FiberTask::Procedure(
                                                   [context](FiberScheduler& scheduler)
                                                   {
                                                       SC_TRY(scheduler.wait(*context->counter));
                                                       *context->runs += 1;
                                                       return Result(true);
                                                   })));
                SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
                SC_TEST_EXPECT(tasks[idx].status() == FiberTaskStatus::Waiting);
            }

            SC_TEST_EXPECT(scheduler.spawn(tasks[2], stacks[2],
                                           FiberTask::Procedure(
                                               [&yieldRuns](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(scheduler.yield());
                                                   yieldRuns += 1;
                                                   return Result(true);
                                               })));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, 2}));
            SC_TEST_EXPECT(tasks[2].status() == FiberTaskStatus::Ready);

            SC_TEST_EXPECT(scheduler.done(counters[0]));
            SC_TEST_EXPECT(scheduler.requestCancel(tasks[0]));
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 2);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 1);

            SC_TEST_EXPECT(scheduler.requestCancel(tasks[1]));
            SC_TEST_EXPECT(scheduler.done(counters[1]));
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 3);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 1);

            SC_TEST_EXPECT(scheduler.requestCancel(tasks[2]));
            SC_TEST_EXPECT(scheduler.readyFiberCount() == 3);
            SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 1);

            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(scheduler.runNoWait(workers[1], {workers, 2}));
            SC_TEST_EXPECT(waitRuns[0] == 0);
            SC_TEST_EXPECT(waitRuns[1] == 0);
            SC_TEST_EXPECT(yieldRuns == 0);
            SC_TEST_EXPECT(not tasks[0].result());
            SC_TEST_EXPECT(not tasks[1].result());
            SC_TEST_EXPECT(not tasks[2].result());
            SC_TEST_EXPECT(scheduler.stolenFiberCount(workers[1]) == 1);
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
            scheduler.releaseWorkerDeques({workers, 2});
            SC_TEST_EXPECT(allocator.close());
        }
    }

    void workerDequeExternalWakeRouting()
    {
        static constexpr size_t NumTasks   = 3;
        static constexpr size_t NumWorkers = 2;

        struct State
        {
            int completed = 0;
        };

        FiberScheduler   scheduler;
        FiberAllocator   allocator;
        char             allocatorStorage[4096] = {};
        FiberWorker      workers[NumWorkers];
        FiberCounter     gate;
        static FiberTask tasks[NumTasks];
        static char      stackMemory[NumTasks][64 * 1024] = {};
        FiberStack       stacks[NumTasks]                 = {
            FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
            FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
            FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
        };
        State state;

        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        SC_TEST_EXPECT(scheduler.createWorkerDeques(allocator, {workers, NumWorkers}, 1));
        scheduler.add(gate);
        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                           FiberTask::Procedure(
                                               [&state, &gate](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(scheduler.wait(gate));
                                                   state.completed += 1;
                                                   return Result(true);
                                               })));
        }

        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(scheduler.runNoWait(workers[0], {workers, NumWorkers}));
            SC_TEST_EXPECT(tasks[idx].status() == FiberTaskStatus::Waiting);
        }
        SC_TEST_EXPECT(scheduler.done(gate));
        SC_TEST_EXPECT(scheduler.readyFiberCount() == NumTasks);
        SC_TEST_EXPECT(scheduler.readyFiberCount(workers[0]) == 0);

        FiberWorkerDiagnostics diagnostics;
        scheduler.workerDiagnostics(workers[0], diagnostics);
        SC_TEST_EXPECT(diagnostics.spilledFibers == 0);

        SC_TEST_EXPECT(scheduler.runReadyFibers(workers[0], {workers, NumWorkers}));
        SC_TEST_EXPECT(state.completed == static_cast<int>(NumTasks));
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        scheduler.releaseWorkerDeques({workers, NumWorkers});
        SC_TEST_EXPECT(allocator.close());
    }

    void workerPoolDeque()
    {
        static constexpr size_t NumWorkers = 4;
        static constexpr size_t NumTasks   = 16;
        static constexpr int    NumYields  = 8;

        struct State
        {
            Atomic<int32_t> completed;
            Atomic<int32_t> resumes[NumTasks];
        };
        struct TaskContext
        {
            State* state = nullptr;
            size_t index = 0;
        };

        FiberScheduler    scheduler;
        FiberAllocator    allocator;
        char              allocatorStorage[8192] = {};
        FiberTask         tasks[NumTasks];
        static char       stackMemory[NumTasks * 64 * 1024] = {};
        FiberTaskPool     taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;
        State             state;
        TaskContext       contexts[NumTasks];

        FiberWorkerPoolOptions badOptions;
        badOptions.dequeCapacityPerWorker = 4;
        Result badStart = workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, badOptions);
        SC_TEST_EXPECT(not badStart);
        SC_TEST_EXPECT(not workerPool.isRunning());

        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        FiberWorkerPoolOptions options;
        options.dequeAllocator         = &allocator;
        options.dequeCapacityPerWorker = 4;

        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            contexts[idx].state  = &state;
            contexts[idx].index  = idx;
            TaskContext* context = &contexts[idx];
            SC_TEST_EXPECT(taskPool.spawn(scheduler, FiberTask::Procedure(
                                                         [context](FiberScheduler& scheduler)
                                                         {
                                                             for (int loop = 0; loop < NumYields; ++loop)
                                                             {
                                                                 context->state->resumes[context->index].fetch_add(1);
                                                                 SC_TRY(scheduler.yield());
                                                             }
                                                             context->state->completed.fetch_add(1);
                                                             return Result(true);
                                                         })));
        }

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, options));
        SC_TEST_EXPECT(allocator.used() > 0);
        SC_TEST_EXPECT(workerPool.join());
        const FiberAllocatorStatistics allocatorStatistics = allocator.statistics();
        FiberWorkerDiagnostics         workerDiagnostics;
        scheduler.workerDiagnostics({workers, NumWorkers}, workerDiagnostics);
        SC_TEST_EXPECT(allocator.used() == 0);
        SC_TEST_EXPECT(allocator.close());
        SC_TEST_EXPECT(allocatorStatistics.peakBytesInUse > 0);
        SC_TEST_EXPECT(allocatorStatistics.numAllocationFailures == 0);
        SC_TEST_EXPECT(workerDiagnostics.executedFibers >= NumTasks);
        SC_TEST_EXPECT(workerDiagnostics.completedFibers == NumTasks);
        SC_TEST_EXPECT(workerDiagnostics.yieldedFibers > 0);
        SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumTasks));
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        for (FiberTask& task : tasks)
        {
            SC_TEST_EXPECT(task.result());
        }
        for (const Atomic<int32_t>& resumes : state.resumes)
        {
            SC_TEST_EXPECT(resumes.load() == NumYields);
        }
    }

    void workerOwnerYieldFastPath()
    {
        static constexpr size_t NumYields = 128;

        FiberScheduler    scheduler;
        FiberAllocator    allocator;
        char              allocatorStorage[4096] = {};
        FiberTask         task;
        static char       stackMemory[64 * 1024] = {};
        FiberStack        stack({stackMemory, sizeof(stackMemory)});
        FiberWorker       worker;
        FiberWorkerThread thread;
        FiberWorkerPool   workerPool;
        Atomic<int32_t>   completedYields;

        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        FiberWorkerPoolOptions options;
        options.dequeAllocator         = &allocator;
        options.dequeCapacityPerWorker = 2;

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&completedYields](FiberScheduler& scheduler)
                                           {
                                               for (size_t yieldIndex = 0; yieldIndex < NumYields; ++yieldIndex)
                                               {
                                                   SC_TRY(scheduler.yield());
                                                   completedYields.fetch_add(1, memory_order_relaxed);
                                               }
                                               return Result(true);
                                           })));
        scheduler.resetSchedulerDiagnostics();
        SC_TEST_EXPECT(workerPool.start(scheduler, {&worker, 1}, {&thread, 1}, options));
        SC_TEST_EXPECT(workerPool.join());

        FiberSchedulerDiagnostics diagnostics;
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(completedYields.load(memory_order_relaxed) == static_cast<int32_t>(NumYields));
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(diagnostics.lockAcquisitions < NumYields / 2);
        SC_TEST_EXPECT(allocator.used() == 0);
        SC_TEST_EXPECT(allocator.close());
    }

    void workerCompletionFastPath()
    {
        static constexpr size_t NumTasks = 32;

        FiberScheduler    scheduler;
        FiberAllocator    allocator;
        char              allocatorStorage[4096] = {};
        static FiberTask  tasks[NumTasks];
        static char       stackMemory[NumTasks * 32 * 1024] = {};
        FiberWorker       worker;
        FiberWorkerThread thread;
        FiberWorkerPool   workerPool;
        Atomic<int32_t>   completed;

        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        FiberWorkerPoolOptions options;
        options.dequeAllocator         = &allocator;
        options.dequeCapacityPerWorker = 4;

        for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
        {
            FiberStack stack({stackMemory + taskIndex * 32 * 1024, 32 * 1024});
            SC_TEST_EXPECT(scheduler.spawn(tasks[taskIndex], stack,
                                           FiberTask::Procedure(
                                               [&completed](FiberScheduler&)
                                               {
                                                   completed.fetch_add(1, memory_order_relaxed);
                                                   return Result(true);
                                               })));
        }
        scheduler.resetSchedulerDiagnostics();
        SC_TEST_EXPECT(workerPool.start(scheduler, {&worker, 1}, {&thread, 1}, options));
        SC_TEST_EXPECT(workerPool.join());

        FiberSchedulerDiagnostics diagnostics;
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(completed.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks));
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(diagnostics.lockAcquisitions < NumTasks + 12);
        for (FiberTask& task : tasks)
        {
            SC_TEST_EXPECT(task.result());
        }
        SC_TEST_EXPECT(allocator.used() == 0);
        SC_TEST_EXPECT(allocator.close());
    }

    void counterCompletionFastPath()
    {
        static constexpr size_t NumThreads     = 4;
        static constexpr size_t NumCompletions = 128;

        struct State
        {
            FiberScheduler* scheduler = nullptr;
            FiberCounter*   counter   = nullptr;

            Atomic<int32_t> successfulCompletions;
            Atomic<int32_t> resumed;
            Atomic<bool>    start;
            Atomic<bool>    failed;
        };
        struct ThreadContext
        {
            State* state = nullptr;
        };

        FiberScheduler scheduler;
        FiberCounter   counter;
        FiberTask      waiter;
        char           waiterStackMemory[64 * 1024] = {};
        FiberStack     waiterStack({waiterStackMemory, sizeof(waiterStackMemory)});
        Thread         threads[NumThreads];
        ThreadContext  contexts[NumThreads];
        State          state;

        state.scheduler = &scheduler;
        state.counter   = &counter;
        for (size_t completionIndex = 0; completionIndex < NumCompletions; ++completionIndex)
        {
            scheduler.add(counter);
        }
        SC_TEST_EXPECT(scheduler.spawn(waiter, waiterStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.wait(*state.counter));
                                               state.resumed.fetch_add(1, memory_order_relaxed);
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(waiter.status() == FiberTaskStatus::Waiting);

        scheduler.resetSchedulerDiagnostics();
        for (size_t threadIndex = 0; threadIndex < NumThreads; ++threadIndex)
        {
            contexts[threadIndex].state = &state;
            ThreadContext* context      = &contexts[threadIndex];
            SC_TEST_EXPECT(threads[threadIndex].start(
                [context](Thread&)
                {
                    while (not context->state->start.load(memory_order_acquire))
                    {
                        Thread::Sleep(1);
                    }
                    for (size_t completionIndex = 0; completionIndex < NumCompletions / NumThreads; ++completionIndex)
                    {
                        if (context->state->scheduler->done(*context->state->counter))
                        {
                            context->state->successfulCompletions.fetch_add(1, memory_order_relaxed);
                        }
                        else
                        {
                            context->state->failed.store(true, memory_order_relaxed);
                        }
                    }
                }));
        }
        state.start.store(true, memory_order_release);
        for (Thread& thread : threads)
        {
            SC_TEST_EXPECT(thread.join());
        }

        FiberSchedulerDiagnostics diagnostics;
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(not state.failed.load(memory_order_relaxed));
        SC_TEST_EXPECT(state.successfulCompletions.load(memory_order_relaxed) == static_cast<int32_t>(NumCompletions));
        SC_TEST_EXPECT(counter.value() == 0);
        SC_TEST_EXPECT(waiter.status() == FiberTaskStatus::Ready);
        SC_TEST_EXPECT(diagnostics.readyFibers == 1);
        SC_TEST_EXPECT(diagnostics.lockAcquisitions <= 3);
        SC_TEST_EXPECT(not scheduler.done(counter));

        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.resumed.load(memory_order_relaxed) == 1);
        SC_TEST_EXPECT(waiter.result());
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
    }

    void workerCounterCompletionFastPath()
    {
        static constexpr size_t NumTasks = 32;

        FiberScheduler    scheduler;
        FiberAllocator    allocator;
        char              allocatorStorage[4096] = {};
        static FiberTask  tasks[NumTasks];
        static char       stackMemory[NumTasks * 32 * 1024] = {};
        FiberCounter      completionCounter;
        FiberWorker       worker;
        FiberWorkerThread thread;
        FiberWorkerPool   workerPool;
        Atomic<int32_t>   completed;

        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        FiberWorkerPoolOptions options;
        options.dequeAllocator         = &allocator;
        options.dequeCapacityPerWorker = 4;

        for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
        {
            FiberStack stack({stackMemory + taskIndex * 32 * 1024, 32 * 1024});
            SC_TEST_EXPECT(scheduler.spawn(tasks[taskIndex], stack,
                                           FiberTask::Procedure(
                                               [&completed](FiberScheduler&)
                                               {
                                                   completed.fetch_add(1, memory_order_relaxed);
                                                   return Result(true);
                                               }),
                                           &completionCounter));
        }
        scheduler.resetSchedulerDiagnostics();
        SC_TEST_EXPECT(workerPool.start(scheduler, {&worker, 1}, {&thread, 1}, options));
        SC_TEST_EXPECT(workerPool.join());

        FiberSchedulerDiagnostics diagnostics;
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(completed.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks));
        SC_TEST_EXPECT(completionCounter.value() == 0);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(diagnostics.lockAcquisitions < NumTasks + 14);
        for (FiberTask& task : tasks)
        {
            SC_TEST_EXPECT(task.result());
        }
        SC_TEST_EXPECT(allocator.used() == 0);
        SC_TEST_EXPECT(allocator.close());
    }

    void workerClaimBatchFastPath()
    {
        static constexpr size_t NumTasks = 32;

        struct State
        {
            size_t          executionOrder[NumTasks] = {};
            Atomic<int32_t> completed;
        };

        FiberScheduler    scheduler;
        FiberAllocator    allocator;
        char              allocatorStorage[4096] = {};
        static FiberTask  tasks[NumTasks];
        static char       stackMemory[NumTasks * 32 * 1024] = {};
        FiberWorker       worker;
        FiberWorkerThread thread;
        FiberWorkerPool   workerPool;
        State             state;

        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        FiberWorkerPoolOptions options;
        options.dequeAllocator         = &allocator;
        options.dequeCapacityPerWorker = 4;

        for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
        {
            FiberStack stack({stackMemory + taskIndex * 32 * 1024, 32 * 1024});
            SC_TEST_EXPECT(scheduler.spawn(tasks[taskIndex], stack,
                                           FiberTask::Procedure(
                                               [&state, taskIndex](FiberScheduler&)
                                               {
                                                   const int32_t executionIndex =
                                                       state.completed.fetch_add(1, memory_order_relaxed);
                                                   state.executionOrder[executionIndex] = taskIndex;
                                                   return Result(true);
                                               })));
        }
        scheduler.resetSchedulerDiagnostics();
        SC_TEST_EXPECT(workerPool.start(scheduler, {&worker, 1}, {&thread, 1}, options));
        SC_TEST_EXPECT(workerPool.join());

        FiberSchedulerDiagnostics diagnostics;
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks));
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(diagnostics.lockAcquisitions < NumTasks / 2 + 12);
        for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
        {
            SC_TEST_EXPECT(tasks[taskIndex].result());
            SC_TEST_EXPECT(state.executionOrder[taskIndex] == taskIndex);
        }
        SC_TEST_EXPECT(allocator.used() == 0);
        SC_TEST_EXPECT(allocator.close());
    }

    void workerPoolInjectionQueue()
    {
        static constexpr size_t NumWorkers        = 2;
        static constexpr size_t NumTasks          = 6;
        static constexpr size_t InjectionCapacity = 2;

        struct State
        {
            Atomic<int32_t> blockersEntered;
            Atomic<int32_t> blockersCompleted;
            Atomic<int32_t> completed;
            Atomic<bool>    allowBlockers;
        };

        FiberScheduler    scheduler;
        FiberAllocator    allocator;
        char              allocatorStorage[4096] = {};
        static FiberTask  tasks[NumTasks];
        static char       stackMemory[NumTasks * 64 * 1024] = {};
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;
        FiberCounter      gate;
        State             state;

        FiberWorkerPoolOptions badOptions;
        badOptions.injectionCapacity = InjectionCapacity;
        Result badStart = workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, badOptions);
        SC_TEST_EXPECT(not badStart);
        SC_TEST_EXPECT(not workerPool.isRunning());

        SC_TEST_EXPECT(allocator.createFixed(allocatorStorage));
        scheduler.add(gate);
        FiberStack waiterStack({stackMemory, 64 * 1024});
        SC_TEST_EXPECT(scheduler.spawn(tasks[0], waiterStack,
                                       FiberTask::Procedure(
                                           [&state, &gate](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.wait(gate));
                                               state.completed.fetch_add(1);
                                               return Result(true);
                                           })));
        for (size_t taskIndex = 1; taskIndex <= NumWorkers; ++taskIndex)
        {
            FiberStack stack({stackMemory + taskIndex * 64 * 1024, 64 * 1024});
            SC_TEST_EXPECT(scheduler.spawn(tasks[taskIndex], stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   state.blockersEntered.fetch_add(1, memory_order_release);
                                                   while (not state.allowBlockers.load(memory_order_acquire))
                                                   {
                                                       Thread::Sleep(1);
                                                   }
                                                   state.blockersCompleted.fetch_add(1);
                                                   return Result(true);
                                               })));
        }

        FiberWorkerPoolOptions options;
        options.dequeAllocator         = &allocator;
        options.dequeCapacityPerWorker = InjectionCapacity;
        options.injectionAllocator     = &allocator;
        options.injectionCapacity      = InjectionCapacity;
        options.idleSpinAttempts       = 0;
        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, options));

        bool workersPinned = false;
        for (size_t attempt = 0; attempt < 5000; ++attempt)
        {
            workersPinned = tasks[0].status() == FiberTaskStatus::Waiting and
                            state.blockersEntered.load(memory_order_acquire) == static_cast<int32_t>(NumWorkers);
            if (workersPinned)
            {
                break;
            }
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(workersPinned);
        if (not workersPinned)
        {
            state.allowBlockers.store(true, memory_order_release);
            SC_TEST_EXPECT(scheduler.requestCancelAll());
            SC_TEST_EXPECT(scheduler.done(gate));
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(allocator.close());
            return;
        }

        for (size_t taskIndex = 3; taskIndex < 5; ++taskIndex)
        {
            FiberStack stack({stackMemory + taskIndex * 64 * 1024, 64 * 1024});
            SC_TEST_EXPECT(scheduler.spawn(tasks[taskIndex], stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   state.completed.fetch_add(1);
                                                   return Result(true);
                                               })));
        }
        FiberStack rejectedStack({stackMemory + 5 * 64 * 1024, 64 * 1024});
        Result     rejectedSpawn = scheduler.spawn(tasks[5], rejectedStack,
                                                   FiberTask::Procedure([](FiberScheduler&) { return Result(true); }));
        SC_TEST_EXPECT(not rejectedSpawn);
        SC_TEST_EXPECT(tasks[5].status() == FiberTaskStatus::Invalid);

        SC_TEST_EXPECT(scheduler.done(gate));
        FiberSchedulerDiagnostics diagnostics;
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.readyFibers == 3);
        SC_TEST_EXPECT(diagnostics.activeFibers == 5);
        SC_TEST_EXPECT(diagnostics.injectionCapacity == InjectionCapacity);
        SC_TEST_EXPECT(diagnostics.injectionReady == InjectionCapacity);
        SC_TEST_EXPECT(diagnostics.injectionPeak == InjectionCapacity);
        SC_TEST_EXPECT(diagnostics.injectionSpills == 1);

        state.allowBlockers.store(true, memory_order_release);
        SC_TEST_EXPECT(workerPool.join());
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(state.blockersCompleted.load() == static_cast<int32_t>(NumWorkers));
        SC_TEST_EXPECT(state.completed.load() == 3);
        SC_TEST_EXPECT(diagnostics.injectionCapacity == 0);
        SC_TEST_EXPECT(diagnostics.injectionReady == 0);
        SC_TEST_EXPECT(diagnostics.injectionPeak == InjectionCapacity);
        SC_TEST_EXPECT(diagnostics.injectionSpills == 1);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(allocator.used() == 0);
        SC_TEST_EXPECT(allocator.close());
        for (size_t taskIndex = 0; taskIndex < 5; ++taskIndex)
        {
            SC_TEST_EXPECT(tasks[taskIndex].result());
        }
    }

    void schedulerDiagnostics()
    {
        FiberScheduler            scheduler;
        FiberSchedulerDiagnostics diagnostics;
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.readyFibers == 0);
        SC_TEST_EXPECT(diagnostics.activeFibers == 0);
        SC_TEST_EXPECT(diagnostics.lockAcquisitions > 0);
        SC_TEST_EXPECT(diagnostics.lockContentions <= diagnostics.lockAcquisitions);
        SC_TEST_EXPECT(diagnostics.lockPeakSpinRetries <= diagnostics.lockSpinRetries);

        scheduler.resetSchedulerDiagnostics();
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.lockAcquisitions == 1);
        SC_TEST_EXPECT(diagnostics.lockContentions == 0);
        SC_TEST_EXPECT(diagnostics.lockSpinRetries == 0);
        SC_TEST_EXPECT(diagnostics.lockPeakSpinRetries == 0);

        FiberTask  task;
        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});
        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.yield());
                                               return Result(true);
                                           })));

        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.readyFibers == 1);
        SC_TEST_EXPECT(diagnostics.activeFibers == 1);
        SC_TEST_EXPECT(scheduler.run());
        scheduler.schedulerDiagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.readyFibers == 0);
        SC_TEST_EXPECT(diagnostics.activeFibers == 0);
        SC_TEST_EXPECT(diagnostics.lockAcquisitions > 1);
        SC_TEST_EXPECT(task.result());
    }

    void traceHooks()
    {
        struct TraceState
        {
            size_t counts[4]              = {};
            size_t schedulerMatches       = 0;
            size_t workerMatches          = 0;
            size_t taskMatches            = 0;
            size_t completionValues       = 0;
            bool   completionWasProtected = false;
            bool   completionWasActive    = false;

            FiberScheduler* expectedScheduler = nullptr;
            FiberTask*      expectedTask      = nullptr;
        };

        FiberScheduler scheduler;
        FiberTask      task;
        char           stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});
        TraceState     state;

        state.expectedScheduler = &scheduler;
        state.expectedTask      = &task;

        FiberTraceHooks hooks;
        hooks.userData = &state;
        hooks.callback = [](void* userData, const FiberTraceEvent& event)
        {
            TraceState& state = *static_cast<TraceState*>(userData);
            state.counts[static_cast<size_t>(event.type)] += 1;
            if (event.scheduler == state.expectedScheduler)
            {
                state.schedulerMatches += 1;
            }
            if (event.worker != nullptr)
            {
                state.workerMatches += 1;
            }
            if (event.task == state.expectedTask)
            {
                state.taskMatches += 1;
            }
            if (event.type == FiberTraceEventType::TaskCompleted)
            {
                state.completionValues += event.value;
                state.completionWasProtected = event.task->status() == FiberTaskStatus::Completing;
                state.completionWasActive    = event.scheduler->hasActiveFibers();
            }
        };
        scheduler.setTraceHooks(hooks);

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.yield());
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(task.result());

        SC_TEST_EXPECT(state.counts[static_cast<size_t>(FiberTraceEventType::TaskStarted)] == 2);
        SC_TEST_EXPECT(state.counts[static_cast<size_t>(FiberTraceEventType::TaskYielded)] == 1);
        SC_TEST_EXPECT(state.counts[static_cast<size_t>(FiberTraceEventType::TaskWaiting)] == 0);
        SC_TEST_EXPECT(state.counts[static_cast<size_t>(FiberTraceEventType::TaskCompleted)] == 1);
        SC_TEST_EXPECT(state.schedulerMatches == 4);
        SC_TEST_EXPECT(state.workerMatches == 4);
        SC_TEST_EXPECT(state.taskMatches == 4);
        SC_TEST_EXPECT(state.completionValues == 1);
        SC_TEST_EXPECT(state.completionWasProtected);
        SC_TEST_EXPECT(state.completionWasActive);

        scheduler.clearTraceHooks();
    }

    void stackDiagnostics()
    {
        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});
        SC_TEST_EXPECT(stack.sizeInBytes() == sizeof(stackMemory));
        SC_TEST_EXPECT(stack.alignmentWasteInBytes() < static_cast<size_t>(FiberStackAlignment));
        SC_TEST_EXPECT(stack.usableSizeInBytes() >= sizeof(stackMemory) - FiberStackAlignment);
        SC_TEST_EXPECT(stack.isUsable());
        stack.fillHighWaterMark();
        SC_TEST_EXPECT(stack.highWaterUsedBytes() == 0);
        SC_TEST_EXPECT(stack.highWaterUnusedBytes() == stack.usableSizeInBytes());

        char       smallStackMemory[1024] = {};
        FiberStack smallStack({smallStackMemory, sizeof(smallStackMemory)});
        SC_TEST_EXPECT(not smallStack.isUsable());

        FiberScheduler scheduler;
        FiberTask      task;
        SC_TEST_EXPECT(
            scheduler.spawn(task, stack, FiberTask::Procedure([](FiberScheduler&) { return Result(true); })));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(stack.highWaterUsedBytes() > 0);
        SC_TEST_EXPECT(stack.highWaterUnusedBytes() < stack.usableSizeInBytes());

        FiberTask     tasks[1];
        char          poolStackMemory[64 * 1024] = {};
        FiberTaskPool pool({tasks, 1}, {poolStackMemory, sizeof(poolStackMemory)}, 64 * 1024);
        size_t        poolStackUsed = 0;
        size_t        poolStackFree = 0;
        pool.fillHighWaterMarks();
        SC_TEST_EXPECT(pool.spawn(scheduler, FiberTask::Procedure([](FiberScheduler&) { return Result(true); })));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(pool.stackHighWaterUsedBytes(0, poolStackUsed));
        SC_TEST_EXPECT(pool.stackHighWaterUnusedBytes(0, poolStackFree));
        SC_TEST_EXPECT(poolStackUsed > 0);
        SC_TEST_EXPECT(poolStackFree < pool.stackSizeInBytes());
        SC_TEST_EXPECT(not pool.stackHighWaterUsedBytes(1, poolStackUsed));
    }

    void virtualStack()
    {
        FiberVirtualStackOptions options;
        options.usableSizeInBytes = 64 * 1024;

        FiberVirtualStack virtualStack;
        SC_TEST_EXPECT(virtualStack.reserve(options));
        SC_TEST_EXPECT(virtualStack.isReserved());
        SC_TEST_EXPECT(virtualStack.guardSizeInBytes() > 0);
        SC_TEST_EXPECT(virtualStack.usableSizeInBytes() >= options.usableSizeInBytes);
        SC_TEST_EXPECT(virtualStack.reservedSizeInBytes() >=
                       virtualStack.usableSizeInBytes() + virtualStack.guardSizeInBytes());
        SC_TEST_EXPECT(virtualStack.memory().data() != nullptr);
        FiberStack stack = virtualStack.stack();
        SC_TEST_EXPECT(stack.isUsable());

        FiberScheduler scheduler;
        FiberTask      task;
        int            value = 0;
        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&value](FiberScheduler& scheduler)
                                           {
                                               value = 1;
                                               SC_TRY(scheduler.yield());
                                               value = 2;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.activeFiberCount() == 1);
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(scheduler.activeFiberCount() == 0);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(value == 2);

        virtualStack.release();
        SC_TEST_EXPECT(not virtualStack.isReserved());
        SC_TEST_EXPECT(virtualStack.memory().data() == nullptr);
        SC_TEST_EXPECT(virtualStack.usableSizeInBytes() == 0);
    }

    void stackClass()
    {
        SC_TEST_EXPECT(FiberStackSize::FourKiB == FiberStackMinimumSize);
        SC_TEST_EXPECT(FiberStackSize::EightKiB > FiberStackSize::FourKiB);
        SC_TEST_EXPECT(FiberStackSize::ThirtyTwoKiB > FiberStackSize::EightKiB);
        SC_TEST_EXPECT(FiberStackSize::SixtyFourKiB > FiberStackSize::ThirtyTwoKiB);

        FiberStackClassOptions options;
        options.stackSizeInBytes = FiberStackSize::ThirtyTwoKiB;
        options.maxStacks        = 2;
        options.guardPage        = true;

        FiberStackClass stackClass;
        SC_TEST_EXPECT(not stackClass.reserve({}));
        SC_TEST_EXPECT(stackClass.reserve(options));
        SC_TEST_EXPECT(stackClass.isReserved());
        SC_TEST_EXPECT(stackClass.capacity() == 2);
        SC_TEST_EXPECT(stackClass.activeCount() == 0);

        FiberStackClassDiagnostics diagnostics;
        stackClass.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.capacity == 2);
        SC_TEST_EXPECT(diagnostics.activeStacks == 0);
        SC_TEST_EXPECT(diagnostics.peakActiveStacks == 0);
        SC_TEST_EXPECT(diagnostics.stackSizeInBytes >= options.stackSizeInBytes);
        SC_TEST_EXPECT(diagnostics.guardSizeInBytes > 0);
        SC_TEST_EXPECT(diagnostics.reservedSizeBytes >= diagnostics.guardSizeInBytes * options.maxStacks +
                                                            diagnostics.stackSizeInBytes * options.maxStacks);
        SC_TEST_EXPECT(diagnostics.committedSizeBytes > 0);
        SC_TEST_EXPECT(diagnostics.committedSizeBytes < diagnostics.reservedSizeBytes);
        SC_TEST_EXPECT(diagnostics.peakCommittedBytes == diagnostics.committedSizeBytes);

        stackClass.fillHighWaterMarks();
        stackClass.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.highWaterUsedBytes == 0);

        FiberStack firstStack({});
        FiberStack secondStack({});
        FiberStack exhaustedStack({});
        SC_TEST_EXPECT(stackClass.acquire(firstStack));
        SC_TEST_EXPECT(stackClass.acquire(secondStack));
        SC_TEST_EXPECT(firstStack.isUsable());
        SC_TEST_EXPECT(secondStack.isUsable());
        SC_TEST_EXPECT(stackClass.owns(firstStack));
        SC_TEST_EXPECT(stackClass.owns(secondStack));
        SC_TEST_EXPECT(firstStack.memory().data() != secondStack.memory().data());
        SC_TEST_EXPECT(stackClass.activeCount() == 2);
        SC_TEST_EXPECT(not stackClass.acquire(exhaustedStack));

        FiberScheduler scheduler;
        FiberTask      task;
        SC_TEST_EXPECT(scheduler.spawn(task, firstStack,
                                       FiberTask::Procedure(
                                           [](FiberScheduler& scheduler)
                                           {
                                               SC_TRY(scheduler.yield());
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(task.result());

        stackClass.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.activeStacks == 2);
        SC_TEST_EXPECT(diagnostics.peakActiveStacks == 2);
        SC_TEST_EXPECT(diagnostics.committedSizeBytes > diagnostics.peakCommittedBytes / 2);
        SC_TEST_EXPECT(diagnostics.committedSizeBytes == diagnostics.peakCommittedBytes);
        SC_TEST_EXPECT(diagnostics.highWaterUsedBytes > 0);

        SC_TEST_EXPECT(stackClass.release(firstStack));
        SC_TEST_EXPECT(stackClass.release(secondStack));
        SC_TEST_EXPECT(firstStack.memory().data() == nullptr);
        SC_TEST_EXPECT(secondStack.memory().data() == nullptr);
        SC_TEST_EXPECT(stackClass.activeCount() == 0);
        stackClass.diagnostics(diagnostics);
        SC_TEST_EXPECT(diagnostics.committedSizeBytes < diagnostics.peakCommittedBytes);
        SC_TEST_EXPECT(diagnostics.highWaterUsedBytes > 0);

        FiberStack reusedStack({});
        SC_TEST_EXPECT(stackClass.acquire(reusedStack));
        SC_TEST_EXPECT(stackClass.release(reusedStack));
        stackClass.release();
        SC_TEST_EXPECT(not stackClass.isReserved());
    }

    void fiberEvent()
    {
        struct State
        {
            FiberEvent* event = nullptr;
            int         step  = 0;
        };

        FiberScheduler scheduler;
        FiberTask      task;
        FiberEvent     event;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.event = &event;
        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               state.step = 1;
                                               SC_TRY(state.event->wait(scheduler));
                                               state.step = 2;
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(task.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(state.step == 1);
        SC_TEST_EXPECT(event.signal(scheduler));
        SC_TEST_EXPECT(event.isSignaled());
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.step == 2);
        event.reset();
        SC_TEST_EXPECT(not event.isSignaled());
    }

    void fiberAutoResetEvent()
    {
        struct State
        {
            FiberAutoResetEvent* event     = nullptr;
            int                  completed = 0;
        };

        FiberScheduler      scheduler;
        FiberTask           firstTask;
        FiberTask           secondTask;
        FiberAutoResetEvent event(true);
        char                firstStackMemory[64 * 1024]  = {};
        char                secondStackMemory[64 * 1024] = {};
        FiberStack          firstStack({firstStackMemory, sizeof(firstStackMemory)});
        FiberStack          secondStack({secondStackMemory, sizeof(secondStackMemory)});

        SC_TEST_EXPECT(event.isSignaled());
        State state;
        state.event = &event;

        FiberTask::Procedure procedure = FiberTask::Procedure(
            [&state](FiberScheduler& scheduler)
            {
                SC_TRY(state.event->wait(scheduler));
                state.completed += 1;
                return Result(true);
            });

        SC_TEST_EXPECT(scheduler.spawn(firstTask, firstStack, procedure));
        SC_TEST_EXPECT(scheduler.spawn(secondTask, secondStack, procedure));

        SC_TEST_EXPECT(scheduler.runNoWait());
        SC_TEST_EXPECT(state.completed == 1);
        SC_TEST_EXPECT(not event.isSignaled());
        SC_TEST_EXPECT(firstTask.isCompleted());
        SC_TEST_EXPECT(secondTask.status() == FiberTaskStatus::Ready);

        SC_TEST_EXPECT(scheduler.runNoWait());
        SC_TEST_EXPECT(secondTask.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(state.completed == 1);

        SC_TEST_EXPECT(event.signal(scheduler));
        SC_TEST_EXPECT(not event.isSignaled());
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.completed == 2);
        SC_TEST_EXPECT(secondTask.isCompleted());

        SC_TEST_EXPECT(event.signal(scheduler));
        SC_TEST_EXPECT(event.isSignaled());
        event.reset();
        SC_TEST_EXPECT(not event.isSignaled());
    }

    void fiberSemaphore()
    {
        struct State
        {
            FiberSemaphore* semaphore = nullptr;
            int             completed = 0;
        };

        FiberScheduler scheduler;
        FiberTask      firstTask;
        FiberTask      secondTask;
        FiberSemaphore semaphore;
        char           firstStackMemory[64 * 1024]  = {};
        char           secondStackMemory[64 * 1024] = {};
        FiberStack     firstStack({firstStackMemory, sizeof(firstStackMemory)});
        FiberStack     secondStack({secondStackMemory, sizeof(secondStackMemory)});
        State          state;
        state.semaphore = &semaphore;

        FiberTask::Procedure procedure = FiberTask::Procedure(
            [&state](FiberScheduler& scheduler)
            {
                SC_TRY(state.semaphore->wait(scheduler));
                state.completed++;
                return Result(true);
            });

        SC_TEST_EXPECT(scheduler.spawn(firstTask, firstStack, procedure));
        SC_TEST_EXPECT(scheduler.spawn(secondTask, secondStack, procedure));
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(firstTask.status() == FiberTaskStatus::Waiting);
        SC_TEST_EXPECT(secondTask.status() == FiberTaskStatus::Waiting);

        SC_TEST_EXPECT(semaphore.signal(scheduler));
        SC_TEST_EXPECT(scheduler.runOnce());
        SC_TEST_EXPECT(state.completed == 1);
        SC_TEST_EXPECT(firstTask.isCompleted());
        SC_TEST_EXPECT(secondTask.status() == FiberTaskStatus::Waiting);

        SC_TEST_EXPECT(semaphore.signal(scheduler));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.completed == 2);
        SC_TEST_EXPECT(secondTask.isCompleted());
    }

    void fiberMutex()
    {
        struct State
        {
            FiberMutex* mutex = nullptr;
            int         value = 0;
        };

        FiberScheduler scheduler;
        FiberTask      firstTask;
        FiberTask      secondTask;
        FiberMutex     mutex;

        char       firstStackMemory[64 * 1024]  = {};
        char       secondStackMemory[64 * 1024] = {};
        FiberStack firstStack({firstStackMemory, sizeof(firstStackMemory)});
        FiberStack secondStack({secondStackMemory, sizeof(secondStackMemory)});

        State state;
        state.mutex                    = &mutex;
        FiberTask::Procedure procedure = FiberTask::Procedure(
            [&state](FiberScheduler& scheduler)
            {
                SC_TRY(state.mutex->lock(scheduler));
                const int value = state.value;
                SC_TRY(scheduler.yield());
                state.value = value + 1;
                SC_TRY(state.mutex->unlock(scheduler));
                return Result(true);
            });

        SC_TEST_EXPECT(scheduler.spawn(firstTask, firstStack, procedure));
        SC_TEST_EXPECT(scheduler.spawn(secondTask, secondStack, procedure));
        SC_TEST_EXPECT(scheduler.run());
        SC_TEST_EXPECT(state.value == 2);
        SC_TEST_EXPECT(not mutex.isLocked());
        SC_TEST_EXPECT(firstTask.result());
        SC_TEST_EXPECT(secondTask.result());
    }

    void multiWorkerPrimitives()
    {
        static constexpr size_t NumWorkers    = 4;
        static constexpr size_t NumWaiters    = 4;
        static constexpr size_t NumMutexTasks = 4;
        static constexpr int    NumMutexLoops = 8;

        {
            struct State
            {
                FiberScheduler* scheduler = nullptr;
                FiberEvent*     event     = nullptr;
                Atomic<int32_t> started;
                Atomic<int32_t> completed;
                Atomic<bool>    workerSucceeded[NumWorkers];
            };

            FiberScheduler scheduler;
            FiberEvent     event;
            FiberTask      tasks[NumWaiters + 1];
            static char    stackMemory[NumWaiters + 1][64 * 1024] = {};
            FiberStack     stacks[NumWaiters + 1]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
                FiberStack({stackMemory[3], sizeof(stackMemory[3])}),
                FiberStack({stackMemory[4], sizeof(stackMemory[4])}),
            };

            State state;
            state.scheduler = &scheduler;
            state.event     = &event;
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                state.workerSucceeded[idx].store(true);
            }

            for (size_t idx = 0; idx < NumWaiters; ++idx)
            {
                SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                               FiberTask::Procedure(
                                                   [&state](FiberScheduler& scheduler)
                                                   {
                                                       state.started.fetch_add(1);
                                                       SC_TRY(state.event->wait(scheduler));
                                                       state.completed.fetch_add(1);
                                                       return Result(true);
                                                   })));
            }
            SC_TEST_EXPECT(scheduler.spawn(tasks[NumWaiters], stacks[NumWaiters],
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   while (state.started.load() != static_cast<int32_t>(NumWaiters))
                                                   {
                                                       SC_TRY(scheduler.yield());
                                                   }
                                                   return state.event->signal(scheduler);
                                               })));

            Thread workers[NumWorkers];
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                SC_TEST_EXPECT(workers[idx].start(
                    [&state, idx](Thread&)
                    {
                        while (state.scheduler->hasActiveFibers())
                        {
                            Result result = state.scheduler->runNoWait();
                            if (not result)
                            {
                                state.workerSucceeded[idx].store(false);
                                return;
                            }
                        }
                    }));
            }
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                SC_TEST_EXPECT(workers[idx].join());
                SC_TEST_EXPECT(state.workerSucceeded[idx].load());
            }
            SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumWaiters));
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                FiberScheduler* scheduler = nullptr;
                FiberSemaphore* semaphore = nullptr;
                Atomic<int32_t> started;
                Atomic<int32_t> completed;
                Atomic<bool>    workerSucceeded[NumWorkers];
            };

            FiberScheduler scheduler;
            FiberSemaphore semaphore;
            FiberTask      tasks[NumWaiters + 1];
            static char    stackMemory[NumWaiters + 1][64 * 1024] = {};
            FiberStack     stacks[NumWaiters + 1]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
                FiberStack({stackMemory[3], sizeof(stackMemory[3])}),
                FiberStack({stackMemory[4], sizeof(stackMemory[4])}),
            };

            State state;
            state.scheduler = &scheduler;
            state.semaphore = &semaphore;
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                state.workerSucceeded[idx].store(true);
            }

            for (size_t idx = 0; idx < NumWaiters; ++idx)
            {
                SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                               FiberTask::Procedure(
                                                   [&state](FiberScheduler& scheduler)
                                                   {
                                                       state.started.fetch_add(1);
                                                       SC_TRY(state.semaphore->wait(scheduler));
                                                       state.completed.fetch_add(1);
                                                       return Result(true);
                                                   })));
            }
            SC_TEST_EXPECT(scheduler.spawn(tasks[NumWaiters], stacks[NumWaiters],
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   while (state.started.load() != static_cast<int32_t>(NumWaiters))
                                                   {
                                                       SC_TRY(scheduler.yield());
                                                   }
                                                   return state.semaphore->signal(scheduler, NumWaiters);
                                               })));

            Thread workers[NumWorkers];
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                SC_TEST_EXPECT(workers[idx].start(
                    [&state, idx](Thread&)
                    {
                        while (state.scheduler->hasActiveFibers())
                        {
                            Result result = state.scheduler->runNoWait();
                            if (not result)
                            {
                                state.workerSucceeded[idx].store(false);
                                return;
                            }
                        }
                    }));
            }
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                SC_TEST_EXPECT(workers[idx].join());
                SC_TEST_EXPECT(state.workerSucceeded[idx].load());
            }
            SC_TEST_EXPECT(state.completed.load() == static_cast<int32_t>(NumWaiters));
            SC_TEST_EXPECT(semaphore.available() == 0);
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }

        {
            struct State
            {
                FiberScheduler* scheduler = nullptr;
                FiberMutex*     mutex     = nullptr;
                Atomic<bool>    workerSucceeded[NumWorkers];
                int             value = 0;
            };

            FiberScheduler scheduler;
            FiberMutex     mutex;
            FiberTask      tasks[NumMutexTasks];
            static char    stackMemory[NumMutexTasks][64 * 1024] = {};
            FiberStack     stacks[NumMutexTasks]                 = {
                FiberStack({stackMemory[0], sizeof(stackMemory[0])}),
                FiberStack({stackMemory[1], sizeof(stackMemory[1])}),
                FiberStack({stackMemory[2], sizeof(stackMemory[2])}),
                FiberStack({stackMemory[3], sizeof(stackMemory[3])}),
            };

            State state;
            state.scheduler = &scheduler;
            state.mutex     = &mutex;
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                state.workerSucceeded[idx].store(true);
            }

            for (size_t idx = 0; idx < NumMutexTasks; ++idx)
            {
                SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stacks[idx],
                                               FiberTask::Procedure(
                                                   [&state](FiberScheduler& scheduler)
                                                   {
                                                       for (int loop = 0; loop < NumMutexLoops; ++loop)
                                                       {
                                                           SC_TRY(state.mutex->lock(scheduler));
                                                           const int value = state.value;
                                                           SC_TRY(scheduler.yield());
                                                           state.value = value + 1;
                                                           SC_TRY(state.mutex->unlock(scheduler));
                                                           SC_TRY(scheduler.yield());
                                                       }
                                                       return Result(true);
                                                   })));
            }

            Thread workers[NumWorkers];
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                SC_TEST_EXPECT(workers[idx].start(
                    [&state, idx](Thread&)
                    {
                        while (state.scheduler->hasActiveFibers())
                        {
                            Result result = state.scheduler->runNoWait();
                            if (not result)
                            {
                                state.workerSucceeded[idx].store(false);
                                return;
                            }
                        }
                    }));
            }
            for (size_t idx = 0; idx < NumWorkers; ++idx)
            {
                SC_TEST_EXPECT(workers[idx].join());
                SC_TEST_EXPECT(state.workerSucceeded[idx].load());
            }
            SC_TEST_EXPECT(state.value == static_cast<int>(NumMutexTasks * NumMutexLoops));
            SC_TEST_EXPECT(not mutex.isLocked());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        }
    }

    void primitiveCancellation()
    {
        {
            struct State
            {
                FiberEvent* event    = nullptr;
                bool        canceled = false;
            };

            FiberScheduler scheduler;
            FiberTask      task;
            FiberEvent     event;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});
            State          state;
            state.event = &event;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   Result result  = state.event->wait(scheduler);
                                                   state.canceled = not result;
                                                   return result;
                                               })));
            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(task.status() == FiberTaskStatus::Waiting);
            SC_TEST_EXPECT(scheduler.requestCancel(task));
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(not task.result());
        }

        {
            struct State
            {
                FiberSemaphore* semaphore = nullptr;
                bool            canceled  = false;
            };

            FiberScheduler scheduler;
            FiberTask      task;
            FiberSemaphore semaphore;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});
            State          state;
            state.semaphore = &semaphore;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   Result result  = state.semaphore->wait(scheduler);
                                                   state.canceled = not result;
                                                   return result;
                                               })));
            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(task.status() == FiberTaskStatus::Waiting);
            SC_TEST_EXPECT(semaphore.signal(scheduler));
            SC_TEST_EXPECT(scheduler.requestCancel(task));
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(semaphore.available() == 1);
            SC_TEST_EXPECT(not task.result());
        }

        {
            struct State
            {
                FiberMutex* mutex    = nullptr;
                bool        canceled = false;
            };

            FiberScheduler scheduler;
            FiberTask      ownerTask;
            FiberTask      waiterTask;
            FiberMutex     mutex;
            char           ownerStackMemory[64 * 1024]  = {};
            char           waiterStackMemory[64 * 1024] = {};
            FiberStack     ownerStack({ownerStackMemory, sizeof(ownerStackMemory)});
            FiberStack     waiterStack({waiterStackMemory, sizeof(waiterStackMemory)});
            State          state;
            state.mutex = &mutex;

            SC_TEST_EXPECT(scheduler.spawn(ownerTask, ownerStack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   SC_TRY(state.mutex->lock(scheduler));
                                                   SC_TRY(scheduler.yield());
                                                   SC_TRY(state.mutex->unlock(scheduler));
                                                   return Result(true);
                                               })));
            SC_TEST_EXPECT(scheduler.spawn(waiterTask, waiterStack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler& scheduler)
                                               {
                                                   Result result  = state.mutex->lock(scheduler);
                                                   state.canceled = not result;
                                                   return result;
                                               })));

            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(mutex.isLocked());
            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(waiterTask.status() == FiberTaskStatus::Waiting);
            SC_TEST_EXPECT(scheduler.runOnce());
            SC_TEST_EXPECT(ownerTask.isCompleted());
            SC_TEST_EXPECT(mutex.isLocked());
            SC_TEST_EXPECT(scheduler.requestCancel(waiterTask));
            SC_TEST_EXPECT(scheduler.run());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(not mutex.isLocked());
            SC_TEST_EXPECT(not waiterTask.result());
        }
    }
};

namespace SC
{
void runFibersTest(SC::TestReport& report) { FibersTest test(report); }
} // namespace SC
