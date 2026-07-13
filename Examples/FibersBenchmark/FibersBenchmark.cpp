// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// Standalone Fibers scheduler benchmark for comparing worker-pool and deque changes without polluting normal tests.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Fibers/Fibers.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Threading/Atomic.h"
#include "../../Libraries/Threading/Threading.h"
#include "../../Libraries/Time/Time.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace SC
{
static size_t availableHardwareWorkers()
{
#if SC_PLATFORM_WINDOWS
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors == 0 ? 1 : static_cast<size_t>(info.dwNumberOfProcessors);
#else
    const long processors = sysconf(_SC_NPROCESSORS_ONLN);
    return processors <= 0 ? 1 : static_cast<size_t>(processors);
#endif
}

static bool hasWorkerCount(Span<size_t> counts, size_t count)
{
    for (size_t existing : counts)
    {
        if (existing == count)
        {
            return true;
        }
    }
    return false;
}

static const char* benchmarkPlatformName()
{
#if SC_PLATFORM_WINDOWS
    return "Windows";
#elif SC_PLATFORM_APPLE
    return "macOS";
#elif SC_PLATFORM_LINUX
    return "Linux";
#else
    return "Unknown";
#endif
}

static const char* benchmarkArchitectureName()
{
#if SC_PLATFORM_ARM64
    return "ARM64";
#elif SC_PLATFORM_INTEL and SC_PLATFORM_64_BIT
    return "x86_64";
#else
    return "Unknown";
#endif
}

static const char* benchmarkCompilerName()
{
#if defined(_MSC_VER)
    return "MSVC";
#elif defined(__clang__)
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#else
    return "Unknown";
#endif
}

static void printBenchmarkEnvironment(Console& console)
{
    console.print("FibersBenchmark environment: platform={} architecture={} compiler={} hardwareWorkers={}\n",
                  benchmarkPlatformName(), benchmarkArchitectureName(), benchmarkCompilerName(),
                  availableHardwareWorkers());
}

static Result runWorkerPoolBenchmark(Console& console)
{
    static constexpr size_t NumWorkers             = 4;
    static constexpr size_t NumTasks               = 256;
    static constexpr size_t StackSize              = 64 * 1024;
    static constexpr size_t DequeCapacityPerWorker = 256;
    static constexpr int    NumYields              = 4096;

    struct State
    {
        Atomic<int32_t> completed;
        Atomic<int32_t> totalYields;
    };

    FiberScheduler    scheduler;
    FiberWorker       workers[NumWorkers];
    FiberWorkerThread threads[NumWorkers];
    FiberWorkerPool   workerPool;

    FiberTask     tasks[NumTasks];
    static char   stackMemory[NumTasks * StackSize] = {};
    FiberTaskPool taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, StackSize);

    FiberAllocator allocator;
    static char    dequeStorage[64 * 1024] = {};

    FiberWorkerPoolOptions workerPoolOptions;
    workerPoolOptions.dequeAllocator         = &allocator;
    workerPoolOptions.dequeCapacityPerWorker = DequeCapacityPerWorker;

    State state;

    SC_TRY(allocator.createFixed(dequeStorage));
    taskPool.fillHighWaterMarks();

    for (size_t idx = 0; idx < NumTasks; ++idx)
    {
        SC_TRY(taskPool.spawn(scheduler, FiberTask::Procedure(
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
    SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, workerPoolOptions));
    SC_TRY(workerPool.join());
    Time::HighResolutionCounter finish;
    finish.snap();

    const FiberAllocatorStatistics allocatorStatistics = allocator.statistics();
    SC_TRY(allocator.close());

    const Time::HighResolutionCounter elapsed      = finish.subtractExact(start);
    const int64_t                     elapsedNs    = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const int64_t                     elapsedMs    = elapsed.toMilliseconds().ms;
    const int64_t                     completed    = state.completed.load();
    const int64_t                     totalYields  = state.totalYields.load();
    const int64_t                     tasksPerSec  = completed * 1000000000 / elapsedNs;
    const int64_t                     yieldsPerSec = totalYields * 1000000000 / elapsedNs;

    FiberWorkerDiagnostics workerDiagnostics;
    scheduler.workerDiagnostics({workers, NumWorkers}, workerDiagnostics);

    FiberSchedulerDiagnostics schedulerDiagnostics;
    scheduler.schedulerDiagnostics(schedulerDiagnostics);

    size_t maxStackUsed = 0;
    for (size_t idx = 0; idx < taskPool.capacity(); ++idx)
    {
        size_t usedBytes = 0;
        SC_TRY(taskPool.stackHighWaterUsedBytes(idx, usedBytes));
        if (usedBytes > maxStackUsed)
        {
            maxStackUsed = usedBytes;
        }
    }

    console.print("FibersBenchmark worker-pool/deque\n");
    console.print("  workers={} tasks={} yieldsPerTask={} totalYields={}\n", NumWorkers, NumTasks,
                  static_cast<size_t>(NumYields), static_cast<size_t>(totalYields));
    console.print("  dequeCapacityPerWorker={}\n", DequeCapacityPerWorker);
    console.print("  elapsedMs={} elapsedNs={}\n", static_cast<size_t>(elapsedMs), static_cast<size_t>(elapsedNs));
    console.print("  completed={} tasksPerSec={} yieldsPerSec={}\n", static_cast<size_t>(completed),
                  static_cast<size_t>(tasksPerSec), static_cast<size_t>(yieldsPerSec));
    console.print("  steals={} failedSteals={} queuePeak={} spilled={}\n", workerDiagnostics.stolenFibers,
                  workerDiagnostics.failedSteals, workerDiagnostics.readyPeakFibers, workerDiagnostics.spilledFibers);
    console.print("  runAttempts={} idlePolls={} executedFibers={} completedFibers={}\n", workerDiagnostics.runAttempts,
                  workerDiagnostics.idlePolls, workerDiagnostics.executedFibers, workerDiagnostics.completedFibers);
    console.print("  yieldedFibers={} waitingFibers={}\n", workerDiagnostics.yieldedFibers,
                  workerDiagnostics.waitingFibers);
    console.print("  schedulerLockContentions={} schedulerLockSpinRetries={} schedulerLockPeakSpinRetries={}\n",
                  schedulerDiagnostics.lockContentions, schedulerDiagnostics.lockSpinRetries,
                  schedulerDiagnostics.lockPeakSpinRetries);
    console.print("  allocatorPeakBytes={} allocatorFailures={} maxStackUsed={}\n", allocatorStatistics.peakBytesInUse,
                  allocatorStatistics.numAllocationFailures, maxStackUsed);

    return Result(true);
}

static Result runForcedStealingBenchmark(Console& console)
{
    static constexpr size_t NumWorkers             = 4;
    static constexpr size_t NumTasks               = 256;
    static constexpr size_t StackSize              = 64 * 1024;
    static constexpr size_t DequeCapacityPerWorker = NumTasks;
    static constexpr int    WorkIterations         = 20000;

    struct State
    {
        FiberCounter*   gate = nullptr;
        Atomic<int32_t> waiting;
        Atomic<int32_t> completed;
        Atomic<int32_t> checksum;
    };

    FiberScheduler    scheduler;
    FiberWorker       workers[NumWorkers];
    FiberWorkerThread threads[NumWorkers];
    FiberWorkerPool   workerPool;

    FiberTask     tasks[NumTasks];
    static char   stackMemory[NumTasks * StackSize] = {};
    FiberTaskPool taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, StackSize);

    FiberTask      releaseTask;
    char           releaseStackMemory[StackSize] = {};
    FiberStack     releaseStack({releaseStackMemory, sizeof(releaseStackMemory)});
    FiberCounter   gate;
    FiberAllocator allocator;
    static char    dequeStorage[NumWorkers * DequeCapacityPerWorker * sizeof(FiberTask*) + 4096] = {};
    State          state;
    state.gate = &gate;

    SC_TRY(allocator.createFixed(dequeStorage));
    SC_TRY(scheduler.createWorkerDeques(allocator, {workers, NumWorkers}, DequeCapacityPerWorker));
    scheduler.add(gate);

    for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
    {
        SC_TRY(taskPool.spawn(scheduler, FiberTask::Procedure(
                                             [&state, taskIndex](FiberScheduler& runningScheduler)
                                             {
                                                 state.waiting.fetch_add(1, memory_order_relaxed);
                                                 SC_TRY(runningScheduler.wait(*state.gate));

                                                 uint32_t value = static_cast<uint32_t>(taskIndex + 1);
                                                 for (int iteration = 0; iteration < WorkIterations; ++iteration)
                                                 {
                                                     value = value * 1664525u + 1013904223u;
                                                 }
                                                 state.checksum.fetch_add(static_cast<int32_t>(value),
                                                                          memory_order_relaxed);
                                                 state.completed.fetch_add(1, memory_order_relaxed);
                                                 return Result(true);
                                             })));
        SC_TRY(scheduler.runNoWait(workers[0], {workers, NumWorkers}));
    }

    SC_TRY_MSG(state.waiting.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks),
               "Forced-steal benchmark tasks did not reach the gate");
    SC_TRY(scheduler.spawn(
        releaseTask, releaseStack,
        FiberTask::Procedure([&gate](FiberScheduler& runningScheduler) { return runningScheduler.done(gate); })));
    SC_TRY(scheduler.runNoWait(workers[0], {workers, NumWorkers}));
    SC_TRY_MSG(scheduler.readyFiberCount(workers[0]) == NumTasks,
               "Forced-steal benchmark did not prepare worker zero's local backlog");

    Time::HighResolutionCounter start;
    start.snap();
    SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
    SC_TRY(workerPool.join());
    Time::HighResolutionCounter finish;
    finish.snap();

    FiberWorkerDiagnostics diagnostics;
    scheduler.workerDiagnostics({workers, NumWorkers}, diagnostics);
    SC_TRY_MSG(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks),
               "Forced-steal benchmark did not complete every task");
    SC_TRY_MSG(diagnostics.stolenFibers > 0, "Forced-steal benchmark did not steal work");
    SC_TRY_MSG(taskPool.availableCount() == NumTasks, "Forced-steal benchmark task pool did not fully recycle");
    SC_TRY_MSG(not scheduler.hasActiveFibers(), "Forced-steal benchmark left active fibers");

    const Time::HighResolutionCounter elapsed     = finish.subtractExact(start);
    const int64_t                     elapsedNs   = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const int64_t                     elapsedMs   = elapsed.toMilliseconds().ms;
    const int64_t                     tasksPerSec = static_cast<int64_t>(NumTasks) * 1000000000 / elapsedNs;

    console.print("FibersBenchmark forced stealing\n");
    console.print("  workers={} preparedLocalBacklog={} workIterations={}\n", NumWorkers, NumTasks,
                  static_cast<size_t>(WorkIterations));
    console.print("  elapsedMs={} elapsedNs={} tasksPerSec={} completed={} checksum={}\n",
                  static_cast<size_t>(elapsedMs), static_cast<size_t>(elapsedNs), static_cast<size_t>(tasksPerSec),
                  static_cast<size_t>(state.completed.load(memory_order_relaxed)),
                  static_cast<size_t>(state.checksum.load(memory_order_relaxed)));
    console.print("  stealAttempts={} stolenFibers={} failedSteals={} queuePeak={} spilled={}\n",
                  diagnostics.stealAttempts, diagnostics.stolenFibers, diagnostics.failedSteals,
                  diagnostics.readyPeakFibers, diagnostics.spilledFibers);
    for (size_t workerIndex = 0; workerIndex < NumWorkers; ++workerIndex)
    {
        FiberWorkerDiagnostics workerDiagnostics;
        scheduler.workerDiagnostics(workers[workerIndex], workerDiagnostics);
        console.print("  worker[{}] executed={} completed={} steals={} failedSteals={}\n", workerIndex,
                      workerDiagnostics.executedFibers, workerDiagnostics.completedFibers,
                      workerDiagnostics.stolenFibers, workerDiagnostics.failedSteals);
    }

    scheduler.releaseWorkerDeques({workers, NumWorkers});
    SC_TRY(allocator.close());
    return Result(true);
}

enum class MicroTaskProducerMode
{
    ExternalBeforeWorkers,
    ExternalWhileWorkersRunning,
    InFiberProducer
};

static const char* microTaskProducerModeName(MicroTaskProducerMode mode)
{
    switch (mode)
    {
    case MicroTaskProducerMode::ExternalBeforeWorkers: return "external-before-workers";
    case MicroTaskProducerMode::ExternalWhileWorkersRunning: return "external-while-workers-running";
    case MicroTaskProducerMode::InFiberProducer: return "in-fiber-producer";
    }
    return "unknown";
}

struct MicroTaskBenchmarkState
{
    Atomic<int32_t> submitted;
    Atomic<int32_t> completed;
    Atomic<int32_t> checksum;
    Atomic<bool>    producerDone;
    Result          producerResult = Result(true);
};

struct MicroTaskExternalProducerState
{
    FiberScheduler*          scheduler      = nullptr;
    FiberTaskPool*           taskPool       = nullptr;
    MicroTaskBenchmarkState* benchmarkState = nullptr;
};

static Result runTinyCpuJob(MicroTaskBenchmarkState& state, int workIterations)
{
    int value = 0;
    for (int idx = 0; idx < workIterations; ++idx)
    {
        value += (idx * 3) + 1;
    }
    state.checksum.fetch_add(value, memory_order_relaxed);
    state.completed.fetch_add(1, memory_order_relaxed);
    return Result(true);
}

static Result spawnMicroTaskJob(FiberTaskPool& pool, FiberScheduler& scheduler, MicroTaskBenchmarkState& state,
                                int workIterations)
{
    SC_TRY(pool.spawn(scheduler, FiberTask::Procedure([&state, workIterations](FiberScheduler&)
                                                      { return runTinyCpuJob(state, workIterations); })));
    state.submitted.fetch_add(1, memory_order_relaxed);
    return Result(true);
}

static Result printMicroTaskMetrics(Console& console, MicroTaskProducerMode mode, size_t numWorkers, size_t numJobs,
                                    int workIterations, const Time::HighResolutionCounter& elapsed,
                                    const FiberScheduler& scheduler, Span<FiberWorker> workers,
                                    const FiberAllocatorStatistics& allocatorStatistics,
                                    const MicroTaskBenchmarkState&  state)
{
    const int64_t elapsedNs  = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const int64_t elapsedMs  = elapsed.toMilliseconds().ms;
    const int64_t completed  = state.completed.load(memory_order_relaxed);
    const int64_t jobsPerSec = completed * 1000000000 / elapsedNs;

    FiberWorkerDiagnostics workerDiagnostics;
    scheduler.workerDiagnostics(workers, workerDiagnostics);

    FiberSchedulerDiagnostics schedulerDiagnostics;
    scheduler.schedulerDiagnostics(schedulerDiagnostics);

    console.print("FibersBenchmark micro-tasking: mode={} workers={} jobs={}\n", microTaskProducerModeName(mode),
                  numWorkers, numJobs);
    console.print("  workIterations={} elapsedMs={} elapsedNs={}\n", static_cast<size_t>(workIterations),
                  static_cast<size_t>(elapsedMs), static_cast<size_t>(elapsedNs));
    console.print("  submitted={} completed={} jobsPerSec={} checksum={}\n",
                  static_cast<size_t>(state.submitted.load(memory_order_relaxed)), static_cast<size_t>(completed),
                  static_cast<size_t>(jobsPerSec), static_cast<size_t>(state.checksum.load(memory_order_relaxed)));
    console.print("  runAttempts={} idlePolls={} executedFibers={} completedFibers={}\n", workerDiagnostics.runAttempts,
                  workerDiagnostics.idlePolls, workerDiagnostics.executedFibers, workerDiagnostics.completedFibers);
    console.print("  stealAttempts={} stolenFibers={} failedSteals={}\n", workerDiagnostics.stealAttempts,
                  workerDiagnostics.stolenFibers, workerDiagnostics.failedSteals);
    console.print("  queuePeak={} spilled={}\n", workerDiagnostics.readyPeakFibers, workerDiagnostics.spilledFibers);
    console.print("  schedulerLockAcquisitions={} schedulerLockContentions={} schedulerLockSpinRetries={}\n",
                  schedulerDiagnostics.lockAcquisitions, schedulerDiagnostics.lockContentions,
                  schedulerDiagnostics.lockSpinRetries);
    console.print("  allocatorPeakBytes={} allocatorFailures={}\n", allocatorStatistics.peakBytesInUse,
                  allocatorStatistics.numAllocationFailures);
    return Result(true);
}

static Result runMicroTaskBenchmarkCase(Console& console, MicroTaskProducerMode mode, size_t numWorkers)
{
    static constexpr size_t MaxWorkers             = 16;
    static constexpr size_t NumJobs                = 1024;
    static constexpr size_t InFiberPoolCapacity    = 256;
    static constexpr size_t StackSize              = 32 * 1024;
    static constexpr size_t DequeCapacityPerWorker = 256;
    static constexpr int    WorkIterations         = 16;

    SC_TRY_MSG(numWorkers > 0 and numWorkers <= MaxWorkers, "Invalid micro-task worker count");

    FiberScheduler    scheduler;
    FiberWorker       workers[MaxWorkers];
    FiberWorkerThread threads[MaxWorkers];
    FiberWorkerPool   workerPool;

    FiberAllocator allocator;
    static char    dequeStorage[MaxWorkers * DequeCapacityPerWorker * sizeof(FiberTask*) + 4096] = {};

    FiberWorkerPoolOptions workerPoolOptions;
    workerPoolOptions.dequeAllocator         = &allocator;
    workerPoolOptions.dequeCapacityPerWorker = DequeCapacityPerWorker;

    MicroTaskBenchmarkState state;
    SC_TRY(allocator.createFixed(dequeStorage));

    Time::HighResolutionCounter start;
    Time::HighResolutionCounter finish;

    if (mode == MicroTaskProducerMode::InFiberProducer)
    {
        FiberTask  producerTask;
        char       producerStackMemory[StackSize] = {};
        FiberStack producerStack({producerStackMemory, sizeof(producerStackMemory)});

        static FiberTask tasks[InFiberPoolCapacity];
        static char      stackMemory[InFiberPoolCapacity * StackSize] = {};
        FiberTaskPool    taskPool({tasks, InFiberPoolCapacity}, {stackMemory, sizeof(stackMemory)}, StackSize);

        SC_TRY(scheduler.spawn(
            producerTask, producerStack,
            FiberTask::Procedure(
                [&state, &taskPool](FiberScheduler& scheduler)
                {
                    while (state.submitted.load(memory_order_relaxed) < static_cast<int32_t>(NumJobs))
                    {
                        Result spawnResult = spawnMicroTaskJob(taskPool, scheduler, state, WorkIterations);
                        if (spawnResult)
                        {
                            continue;
                        }
                        SC_TRY(taskPool.waitForAvailableTask(scheduler));
                    }
                    state.producerDone.store(true, memory_order_release);
                    return Result(true);
                })));

        start.snap();
        SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, workerPoolOptions));
        SC_TRY(workerPool.join());
        finish.snap();
    }
    else
    {
        static FiberTask tasks[NumJobs];
        static char      stackMemory[NumJobs * StackSize] = {};
        FiberTaskPool    taskPool({tasks, NumJobs}, {stackMemory, sizeof(stackMemory)}, StackSize);

        if (mode == MicroTaskProducerMode::ExternalBeforeWorkers)
        {
            start.snap();
            for (size_t idx = 0; idx < NumJobs; ++idx)
            {
                SC_TRY(spawnMicroTaskJob(taskPool, scheduler, state, WorkIterations));
            }
            state.producerDone.store(true, memory_order_release);
            SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, workerPoolOptions));
            SC_TRY(workerPool.join());
            finish.snap();
        }
        else
        {
            FiberTask  keepAliveTask;
            char       keepAliveStackMemory[StackSize] = {};
            FiberStack keepAliveStack({keepAliveStackMemory, sizeof(keepAliveStackMemory)});

            SC_TRY(scheduler.spawn(keepAliveTask, keepAliveStack,
                                   FiberTask::Procedure(
                                       [&state](FiberScheduler& scheduler)
                                       {
                                           while (not state.producerDone.load(memory_order_acquire))
                                           {
                                               SC_TRY(scheduler.yield());
                                           }
                                           return Result(true);
                                       })));
            SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, workerPoolOptions));

            Thread producerThread;
            start.snap();
            MicroTaskExternalProducerState producerState;
            producerState.scheduler      = &scheduler;
            producerState.taskPool       = &taskPool;
            producerState.benchmarkState = &state;
            SC_TRY(producerThread.start(
                [&producerState](Thread&)
                {
                    for (size_t idx = 0; idx < NumJobs; ++idx)
                    {
                        Result result = spawnMicroTaskJob(*producerState.taskPool, *producerState.scheduler,
                                                          *producerState.benchmarkState, WorkIterations);
                        if (not result)
                        {
                            producerState.benchmarkState->producerResult = result;
                            break;
                        }
                    }
                    producerState.benchmarkState->producerDone.store(true, memory_order_release);
                }));
            SC_TRY(producerThread.join());
            SC_TRY(state.producerResult);
            SC_TRY(workerPool.join());
            finish.snap();
        }
    }

    SC_TRY_MSG(state.submitted.load(memory_order_relaxed) == static_cast<int32_t>(NumJobs),
               "Micro-task benchmark did not submit all jobs");
    SC_TRY_MSG(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(NumJobs),
               "Micro-task benchmark did not complete all jobs");

    const FiberAllocatorStatistics allocatorStatistics = allocator.statistics();
    SC_TRY(allocator.close());

    return printMicroTaskMetrics(console, mode, numWorkers, NumJobs, WorkIterations, finish.subtractExact(start),
                                 scheduler, {workers, numWorkers}, allocatorStatistics, state);
}

static Result runMicroTaskBenchmarks(Console& console)
{
    static constexpr size_t MaxBenchmarkCounts = 5;
    static constexpr size_t MaxWorkers         = 16;

    size_t workerCountsStorage[MaxBenchmarkCounts] = {};
    size_t numWorkerCounts                         = 0;

    const size_t requestedCounts[] = {1, 2, 4, 8, availableHardwareWorkers()};
    for (size_t requestedCount : requestedCounts)
    {
        size_t workerCount = requestedCount > MaxWorkers ? MaxWorkers : requestedCount;
        if (workerCount == 0)
        {
            workerCount = 1;
        }
        Span<size_t> existingCounts({workerCountsStorage, numWorkerCounts});
        if (not hasWorkerCount(existingCounts, workerCount))
        {
            workerCountsStorage[numWorkerCounts++] = workerCount;
        }
    }

    const MicroTaskProducerMode modes[] = {
        MicroTaskProducerMode::ExternalBeforeWorkers,
        MicroTaskProducerMode::ExternalWhileWorkersRunning,
        MicroTaskProducerMode::InFiberProducer,
    };

    for (MicroTaskProducerMode mode : modes)
    {
        for (size_t idx = 0; idx < numWorkerCounts; ++idx)
        {
            SC_TRY(runMicroTaskBenchmarkCase(console, mode, workerCountsStorage[idx]));
        }
    }
    return Result(true);
}

static Result runSustainedMicroTaskBenchmark(Console& console)
{
    static constexpr size_t MaxWorkers             = 16;
    static constexpr size_t NumJobs                = 1000000;
    static constexpr size_t PoolCapacity           = 512;
    static constexpr size_t StackSize              = 32 * 1024;
    static constexpr size_t DequeCapacityPerWorker = 256;
    static constexpr int    WorkIterations         = 4;

    size_t numWorkers = availableHardwareWorkers();
    if (numWorkers > 8)
    {
        numWorkers = 8;
    }
    if (numWorkers > MaxWorkers)
    {
        numWorkers = MaxWorkers;
    }
    if (numWorkers == 0)
    {
        numWorkers = 1;
    }

    FiberScheduler    scheduler;
    FiberWorker       workers[MaxWorkers];
    FiberWorkerThread threads[MaxWorkers];
    FiberWorkerPool   workerPool;

    FiberAllocator allocator;
    static char    dequeStorage[MaxWorkers * DequeCapacityPerWorker * sizeof(FiberTask*) + 4096] = {};

    FiberWorkerPoolOptions workerPoolOptions;
    workerPoolOptions.dequeAllocator         = &allocator;
    workerPoolOptions.dequeCapacityPerWorker = DequeCapacityPerWorker;

    static FiberTask tasks[PoolCapacity];
    static char      stackMemory[PoolCapacity * StackSize] = {};
    FiberTaskPool    taskPool({tasks, PoolCapacity}, {stackMemory, sizeof(stackMemory)}, StackSize);

    FiberTask  producerTask;
    char       producerStackMemory[StackSize] = {};
    FiberStack producerStack({producerStackMemory, sizeof(producerStackMemory)});

    MicroTaskBenchmarkState state;
    SC_TRY(allocator.createFixed(dequeStorage));

    SC_TRY(scheduler.spawn(producerTask, producerStack,
                           FiberTask::Procedure(
                               [&state, &taskPool](FiberScheduler& scheduler)
                               {
                                   while (state.submitted.load(memory_order_relaxed) < static_cast<int32_t>(NumJobs))
                                   {
                                       Result spawnResult =
                                           spawnMicroTaskJob(taskPool, scheduler, state, WorkIterations);
                                       if (spawnResult)
                                       {
                                           continue;
                                       }
                                       SC_TRY(taskPool.waitForAvailableTask(scheduler));
                                   }
                                   state.producerDone.store(true, memory_order_release);
                                   return Result(true);
                               })));

    Time::HighResolutionCounter start;
    start.snap();
    SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, workerPoolOptions));
    SC_TRY(workerPool.join());
    Time::HighResolutionCounter finish;
    finish.snap();

    SC_TRY_MSG(state.submitted.load(memory_order_relaxed) == static_cast<int32_t>(NumJobs),
               "Sustained micro-task benchmark did not submit all jobs");
    SC_TRY_MSG(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(NumJobs),
               "Sustained micro-task benchmark did not complete all jobs");

    const FiberAllocatorStatistics allocatorStatistics = allocator.statistics();
    SC_TRY(allocator.close());

    console.print("FibersBenchmark sustained micro-tasking\n");
    console.print("  poolCapacity={} dequeCapacityPerWorker={}\n", PoolCapacity, DequeCapacityPerWorker);
    return printMicroTaskMetrics(console, MicroTaskProducerMode::InFiberProducer, numWorkers, NumJobs, WorkIterations,
                                 finish.subtractExact(start), scheduler, {workers, numWorkers}, allocatorStatistics,
                                 state);
}

static Result runFibersBenchmark()
{
    Console console;
    Console::tryAttachingToParentConsole();
    printBenchmarkEnvironment(console);
    SC_TRY(runWorkerPoolBenchmark(console));
    SC_TRY(runForcedStealingBenchmark(console));
    SC_TRY(runMicroTaskBenchmarks(console));
    SC_TRY(runSustainedMicroTaskBenchmark(console));
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runFibersBenchmark();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("FibersBenchmark failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
