// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// Standalone Fibers scheduler benchmark for comparing worker-pool and deque changes without polluting normal tests.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/AsyncFibers/AsyncFibers.h"
#include "../../Libraries/Fibers/Fibers.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringFormat.h"
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
    console.print(
        "  runAttempts={} idlePolls={} idleSpinIterations={} parkAttempts={} parkedWakeups={} executedFibers={} "
        "completedFibers={}\n",
        workerDiagnostics.runAttempts, workerDiagnostics.idlePolls, workerDiagnostics.idleSpinIterations,
        workerDiagnostics.parkAttempts, workerDiagnostics.parkedWakeups, workerDiagnostics.executedFibers,
        workerDiagnostics.completedFibers);
    console.print("  yieldedFibers={} waitingFibers={}\n", workerDiagnostics.yieldedFibers,
                  workerDiagnostics.waitingFibers);
    console.print("  schedulerLockContentions={} schedulerLockSpinRetries={} schedulerLockPeakSpinRetries={}\n",
                  schedulerDiagnostics.lockContentions, schedulerDiagnostics.lockSpinRetries,
                  schedulerDiagnostics.lockPeakSpinRetries);
    console.print("  schedulerLocksByCategory: spawn={} ready={} synchronization={} completion={} control={}\n",
                  schedulerDiagnostics.lockSpawn, schedulerDiagnostics.lockReady,
                  schedulerDiagnostics.lockSynchronization, schedulerDiagnostics.lockCompletion,
                  schedulerDiagnostics.lockControl);
    console.print("  injectionLockAcquisitions={} injectionLockContentions={} injectionLockSpinRetries={}\n",
                  schedulerDiagnostics.injectionLockAcquisitions, schedulerDiagnostics.injectionLockContentions,
                  schedulerDiagnostics.injectionLockSpinRetries);
    console.print("  allocatorPeakBytes={} allocatorFailures={} maxStackUsed={}\n", allocatorStatistics.peakBytesInUse,
                  allocatorStatistics.numAllocationFailures, maxStackUsed);

    return Result(true);
}

static Result runFiberJobBenchmark(Console& console)
{
    static constexpr size_t BatchCapacity = 8192;
    static constexpr size_t TotalJobs     = 10'000'000;

    struct State
    {
        size_t completed = 0;
        size_t checksum  = 0;
    };

    static FiberJob   jobs[BatchCapacity];
    static FiberJob*  readyStorage[BatchCapacity] = {};
    FiberJobScheduler scheduler;
    State             state;

    SC_TRY(scheduler.create(readyStorage));
    Time::HighResolutionCounter start;
    start.snap();
    while (state.completed < TotalJobs)
    {
        const size_t remaining = TotalJobs - state.completed;
        const size_t batchSize = remaining < BatchCapacity ? remaining : BatchCapacity;
        for (size_t index = 0; index < batchSize; ++index)
        {
            SC_TRY(scheduler.spawn(jobs[index], FiberJob::Procedure(
                                                    [&state](FiberJobContext&)
                                                    {
                                                        state.checksum += state.completed + 1;
                                                        state.completed += 1;
                                                        return Result(true);
                                                    })));
        }
        SC_TRY(scheduler.run());
    }
    Time::HighResolutionCounter finish;
    finish.snap();
    SC_TRY(scheduler.close());
    SC_TRY_MSG(state.completed == TotalJobs, "FiberJob benchmark did not complete every job");
    SC_TRY_MSG(state.checksum == TotalJobs * (TotalJobs + 1) / 2, "FiberJob benchmark checksum mismatch");

    const Time::HighResolutionCounter elapsed       = finish.subtractExact(start);
    const int64_t                     elapsedNs     = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const size_t                      jobsPerSecond = static_cast<size_t>(TotalJobs * 1000000000ull / elapsedNs);
    console.print("FibersBenchmark stackless jobs\n");
    console.print("  jobs={} batchCapacity={} elapsedNs={} jobsPerSec={} checksum={}\n", TotalJobs, BatchCapacity,
                  static_cast<size_t>(elapsedNs), jobsPerSecond, state.checksum);
    return Result(true);
}

static Result runFiberJobPoolBenchmark(Console& console)
{
    static constexpr size_t BatchCapacity = 8192;
    static constexpr size_t TotalJobs     = 10'000'000;

    struct State
    {
        size_t completed = 0;
        size_t checksum  = 0;
    };

    static FiberJob   jobs[BatchCapacity];
    static FiberJob*  readyStorage[BatchCapacity] = {};
    static FiberJob*  batchJobs[BatchCapacity]    = {};
    FiberJobPool      pool;
    FiberJobScheduler scheduler;
    State             state;

    SC_TRY(pool.create(jobs));
    SC_TRY(scheduler.create(readyStorage));
    Time::HighResolutionCounter start;
    start.snap();
    while (state.completed < TotalJobs)
    {
        const size_t remaining = TotalJobs - state.completed;
        const size_t batchSize = remaining < BatchCapacity ? remaining : BatchCapacity;
        for (size_t index = 0; index < batchSize; ++index)
        {
            SC_TRY(pool.spawn(scheduler,
                              FiberJob::Procedure(
                                  [&state](FiberJobContext&)
                                  {
                                      state.checksum += state.completed + 1;
                                      state.completed += 1;
                                      return Result(true);
                                  }),
                              batchJobs[index]));
        }
        SC_TRY(scheduler.run());
        for (size_t index = 0; index < batchSize; ++index)
        {
            SC_TRY(pool.release(*batchJobs[index]));
            batchJobs[index] = nullptr;
        }
    }
    Time::HighResolutionCounter finish;
    finish.snap();
    SC_TRY(pool.close());
    SC_TRY(scheduler.close());
    SC_TRY_MSG(state.completed == TotalJobs, "FiberJobPool benchmark did not complete every job");
    SC_TRY_MSG(state.checksum == TotalJobs * (TotalJobs + 1) / 2, "FiberJobPool benchmark checksum mismatch");

    const Time::HighResolutionCounter elapsed       = finish.subtractExact(start);
    const int64_t                     elapsedNs     = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const size_t                      jobsPerSecond = static_cast<size_t>(TotalJobs * 1000000000ull / elapsedNs);
    console.print("FibersBenchmark pooled stackless jobs\n");
    console.print("  jobs={} batchCapacity={} elapsedNs={} jobsPerSec={} checksum={}\n", TotalJobs, BatchCapacity,
                  static_cast<size_t>(elapsedNs), jobsPerSecond, state.checksum);
    return Result(true);
}

struct FiberJobWorkerPoolBenchmarkResult
{
    size_t elapsedNanoseconds = 0;
    size_t jobsPerSecond      = 0;
};

static Result runFiberJobWorkerPoolBenchmarkCase(Console& console, size_t numWorkers, bool printDiagnostics,
                                                 FiberJobWorkerPoolBenchmarkResult& benchmarkResult)
{
    static constexpr size_t MaxWorkers             = 64;
    static constexpr size_t TotalJobs              = 1'000'000;
    static constexpr size_t DequeCapacityPerWorker = 256;

    SC_TRY_MSG(numWorkers > 0 and numWorkers <= MaxWorkers, "FiberJob worker count must be between one and 64");

    static FiberJob  jobs[TotalJobs];
    static FiberJob* readyStorage[TotalJobs] = {};
    static uint32_t  results[TotalJobs]      = {};

    FiberJobScheduler         scheduler;
    FiberJobWorker            workers[MaxWorkers];
    FiberJobWorkerThread      threads[MaxWorkers];
    FiberJobWorkerPool        workerPool;
    FiberJobWorkerPoolOptions options;
    FiberAllocator            allocator;
    static char allocatorStorage[MaxWorkers * DequeCapacityPerWorker * sizeof(FiberJob*) + MaxWorkers * 128] = {};

    options.dequeAllocator         = &allocator;
    options.dequeCapacityPerWorker = DequeCapacityPerWorker;

    SC_TRY(allocator.createFixed(allocatorStorage));
    SC_TRY(scheduler.create(readyStorage));
    for (size_t index = 0; index < TotalJobs; ++index)
    {
        uint32_t* result = &results[index];
        SC_TRY(scheduler.spawn(jobs[index], FiberJob::Procedure(
                                                [result, index](FiberJobContext&)
                                                {
                                                    *result = static_cast<uint32_t>(index + 1);
                                                    return Result(true);
                                                })));
    }

    Time::HighResolutionCounter start;
    start.snap();
    SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, options));
    SC_TRY(workerPool.join());
    Time::HighResolutionCounter finish;
    finish.snap();

    size_t checksum = 0;
    for (size_t index = 0; index < TotalJobs; ++index)
    {
        SC_TRY_MSG(results[index] == index + 1, "FiberJob worker-pool benchmark result mismatch");
        checksum += results[index];
        results[index] = 0;
    }
    SC_TRY_MSG(checksum == TotalJobs * (TotalJobs + 1) / 2, "FiberJob worker-pool benchmark checksum mismatch");
    SC_TRY_MSG(not scheduler.hasActiveJobs(), "FiberJob worker-pool benchmark left active jobs");

    size_t stealAttempts = 0;
    size_t stolenJobs    = 0;
    size_t failedSteals  = 0;
    size_t executedJobs  = 0;
    size_t claimBatches  = 0;
    size_t claimedJobs   = 0;
    for (size_t workerIndex = 0; workerIndex < numWorkers; ++workerIndex)
    {
        FiberJobWorkerDiagnostics diagnostics;
        scheduler.workerDiagnostics(workers[workerIndex], diagnostics);
        executedJobs += diagnostics.executedJobs;
        claimBatches += diagnostics.claimBatches;
        claimedJobs += diagnostics.claimedJobs;
        stealAttempts += diagnostics.stealAttempts;
        stolenJobs += diagnostics.stolenJobs;
        failedSteals += diagnostics.failedSteals;
    }

    const FiberAllocatorStatistics allocatorStatistics = allocator.statistics();
    SC_TRY(scheduler.close());
    SC_TRY(allocator.close());
    SC_TRY_MSG(executedJobs == TotalJobs, "FiberJob worker-pool benchmark execution count mismatch");

    const Time::HighResolutionCounter elapsed       = finish.subtractExact(start);
    const int64_t                     elapsedNs     = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const size_t                      jobsPerSecond = static_cast<size_t>(TotalJobs * 1000000000ull / elapsedNs);
    benchmarkResult.elapsedNanoseconds              = static_cast<size_t>(elapsedNs);
    benchmarkResult.jobsPerSecond                   = jobsPerSecond;

    if (printDiagnostics)
    {
        console.print("FibersBenchmark stackless job worker pool\n");
        console.print(
            "  workers={} jobs={} timing=preloaded-start-through-drain elapsedNs={} jobsPerSec={} checksum={}\n",
            numWorkers, TotalJobs, static_cast<size_t>(elapsedNs), jobsPerSecond, checksum);
        console.print(
            "  executedJobs={} claimBatches={} claimedJobs={} stealAttempts={} stolenJobs={} failedSteals={}\n",
            executedJobs, claimBatches, claimedJobs, stealAttempts, stolenJobs, failedSteals);
        console.print("  allocatorPeakBytes={} allocatorFailures={}\n", allocatorStatistics.peakBytesInUse,
                      allocatorStatistics.numAllocationFailures);
        for (size_t workerIndex = 0; workerIndex < numWorkers; ++workerIndex)
        {
            FiberJobWorkerDiagnostics diagnostics;
            scheduler.workerDiagnostics(workers[workerIndex], diagnostics);
            console.print("  worker[{}] executed={} queuePeak={} dequeCapacity={} claimBatches={} claimedJobs={} "
                          "claimBatchPeak={} stealAttempts={} stolenJobs={} failedSteals={}\n",
                          workerIndex, diagnostics.executedJobs, diagnostics.readyPeakJobs, diagnostics.dequeCapacity,
                          diagnostics.claimBatches, diagnostics.claimedJobs, diagnostics.claimBatchPeak,
                          diagnostics.stealAttempts, diagnostics.stolenJobs, diagnostics.failedSteals);
        }
    }
    return Result(true);
}

static Result runFiberJobWorkerPoolBenchmark(Console& console, size_t numWorkers)
{
    FiberJobWorkerPoolBenchmarkResult benchmarkResult;
    return runFiberJobWorkerPoolBenchmarkCase(console, numWorkers, true, benchmarkResult);
}

static void sortBenchmarkSamples(Span<size_t> samples)
{
    for (size_t index = 1; index < samples.sizeInElements(); ++index)
    {
        const size_t sample = samples[index];
        size_t       insert = index;
        while (insert > 0 and samples[insert - 1] > sample)
        {
            samples[insert] = samples[insert - 1];
            --insert;
        }
        samples[insert] = sample;
    }
}

static Result runFiberJobWorkerPoolBenchmarkMatrix(Console& console, size_t numRounds)
{
    static constexpr size_t MaxBenchmarkCounts = 5;
    static constexpr size_t MaxWorkers         = 64;
    static constexpr size_t MaxRounds          = 15;

    SC_TRY_MSG(numRounds > 0 and numRounds <= MaxRounds, "FiberJob benchmark rounds must be between one and 15");

    const size_t hardwareWorkers = availableHardwareWorkers();
    SC_TRY_MSG(hardwareWorkers <= MaxWorkers,
               "FibersBenchmark must increase its fixed FiberJob worker storage for this machine");

    size_t       workerCounts[MaxBenchmarkCounts] = {};
    size_t       numWorkerCounts                  = 0;
    const size_t requestedCounts[]                = {1, 2, 4, 8, hardwareWorkers};
    for (size_t workerCount : requestedCounts)
    {
        Span<size_t> existingCounts({workerCounts, numWorkerCounts});
        if (not hasWorkerCount(existingCounts, workerCount))
        {
            workerCounts[numWorkerCounts++] = workerCount;
        }
    }

    console.print("FibersBenchmark stackless job worker-pool matrix\n");
    console.print("  jobsPerSample=1000000 warmupSamples=1 measuredSamples={} timing=preloaded-start-through-drain\n",
                  numRounds);
    for (size_t countIndex = 0; countIndex < numWorkerCounts; ++countIndex)
    {
        const size_t workerCount = workerCounts[countIndex];

        FiberJobWorkerPoolBenchmarkResult warmupResult;
        SC_TRY(runFiberJobWorkerPoolBenchmarkCase(console, workerCount, false, warmupResult));

        size_t samples[MaxRounds] = {};
        size_t total              = 0;
        for (size_t round = 0; round < numRounds; ++round)
        {
            FiberJobWorkerPoolBenchmarkResult benchmarkResult;
            SC_TRY(runFiberJobWorkerPoolBenchmarkCase(console, workerCount, false, benchmarkResult));
            samples[round] = benchmarkResult.jobsPerSecond;
            total += benchmarkResult.jobsPerSecond;
        }
        sortBenchmarkSamples({samples, numRounds});
        size_t median = samples[numRounds / 2];
        if (numRounds % 2 == 0)
        {
            const size_t lower = samples[numRounds / 2 - 1];
            median             = lower + (median - lower) / 2;
        }
        console.print("  workers={} jobsPerSecMin={} jobsPerSecMedian={} jobsPerSecMean={} jobsPerSecMax={}\n",
                      workerCount, samples[0], median, total / numRounds, samples[numRounds - 1]);
    }
    return Result(true);
}

static Result runSustainedFiberJobBenchmarkCase(Console& console, size_t numWorkers, bool sharedAtomicPayload)
{
    static constexpr size_t MaxWorkers             = 64;
    static constexpr size_t TotalJobs              = 1'000'000;
    static constexpr size_t BatchCapacity          = 8192;
    static constexpr size_t DequeCapacityPerWorker = 256;
    static constexpr int    WorkIterations         = 4;

    struct State
    {
        FiberJob* jobs             = nullptr;
        uint32_t* localCompletions = nullptr;
        uint32_t* localValues      = nullptr;

        Atomic<int32_t> submitted;
        Atomic<int32_t> completed;
        Atomic<int32_t> checksum;
    };

    SC_TRY_MSG(numWorkers > 0 and numWorkers <= MaxWorkers, "FiberJob worker count must be between one and 64");

    static FiberJob  jobs[BatchCapacity];
    static FiberJob* readyStorage[BatchCapacity]     = {};
    static uint32_t  localCompletions[BatchCapacity] = {};
    static uint32_t  localValues[BatchCapacity]      = {};

    FiberJobScheduler         scheduler;
    FiberJobWorker            workers[MaxWorkers];
    FiberJobWorkerThread      threads[MaxWorkers];
    FiberJobWorkerPool        workerPool;
    FiberJobWorkerPoolOptions options;
    FiberAllocator            allocator;
    State                     state;

    static char allocatorStorage[MaxWorkers * DequeCapacityPerWorker * sizeof(FiberJob*) + MaxWorkers * 128] = {};
    options.dequeAllocator         = &allocator;
    options.dequeCapacityPerWorker = DequeCapacityPerWorker;
    options.keepAliveWhenIdle      = true;
    state.jobs                     = jobs;
    state.localCompletions         = localCompletions;
    state.localValues              = localValues;
    for (size_t index = 0; index < BatchCapacity; ++index)
    {
        localCompletions[index] = 0;
        localValues[index]      = 0;
    }

    SC_TRY(allocator.createFixed(allocatorStorage));
    SC_TRY(scheduler.create(readyStorage));
    SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, options));
    while (workerPool.parkedWorkerCount() != numWorkers) {}

    Time::HighResolutionCounter start;
    start.snap();
    while (state.submitted.load(memory_order_relaxed) < static_cast<int32_t>(TotalJobs))
    {
        const size_t submitted = static_cast<size_t>(state.submitted.load(memory_order_relaxed));
        const size_t remaining = TotalJobs - submitted;
        const size_t batchSize = remaining < BatchCapacity ? remaining : BatchCapacity;
        if (sharedAtomicPayload)
        {
            SC_TRY(scheduler.spawn({jobs, batchSize},
                                   FiberJob::Procedure(
                                       [&state](FiberJobContext&)
                                       {
                                           uint32_t value =
                                               static_cast<uint32_t>(state.completed.load(memory_order_relaxed)) + 1;
                                           for (int iteration = 0; iteration < WorkIterations; ++iteration)
                                           {
                                               value =
                                                   (value * 1664525u) ^ static_cast<uint32_t>(iteration + 1013904223u);
                                           }
                                           state.checksum.fetch_add(static_cast<int32_t>(value), memory_order_relaxed);
                                           state.completed.fetch_add(1, memory_order_relaxed);
                                           return Result(true);
                                       })));
        }
        else
        {
            SC_TRY(scheduler.spawn({jobs, batchSize},
                                   FiberJob::Procedure(
                                       [&state](FiberJobContext& context)
                                       {
                                           const size_t index = static_cast<size_t>(&context.job() - state.jobs);
                                           uint32_t     value = state.localValues[index] + 1;
                                           for (int iteration = 0; iteration < WorkIterations; ++iteration)
                                           {
                                               value =
                                                   (value * 1664525u) ^ static_cast<uint32_t>(iteration + 1013904223u);
                                           }
                                           state.localValues[index] = value;
                                           state.localCompletions[index] += 1;
                                           return Result(true);
                                       })));
        }
        state.submitted.fetch_add(static_cast<int32_t>(batchSize), memory_order_relaxed);
        SC_TRY(workerPool.waitIdle());
    }
    Time::HighResolutionCounter finish;
    finish.snap();

    SC_TRY(workerPool.requestStop());
    SC_TRY(workerPool.join());
    SC_TRY_MSG(state.submitted.load(memory_order_relaxed) == static_cast<int32_t>(TotalJobs),
               "Sustained FiberJob benchmark did not submit every job");

    size_t completedJobs = static_cast<size_t>(state.completed.load(memory_order_relaxed));
    size_t checksum      = static_cast<size_t>(state.checksum.load(memory_order_relaxed));
    if (not sharedAtomicPayload)
    {
        completedJobs              = 0;
        checksum                   = 0;
        const size_t completeWaves = TotalJobs / BatchCapacity;
        const size_t remainder     = TotalJobs % BatchCapacity;
        for (size_t index = 0; index < BatchCapacity; ++index)
        {
            const size_t expected = completeWaves + (index < remainder ? 1 : 0);
            SC_TRY_MSG(localCompletions[index] == expected, "Sustained FiberJob independent completion count mismatch");
            completedJobs += localCompletions[index];
            checksum += localValues[index];
        }
    }
    SC_TRY_MSG(completedJobs == TotalJobs, "Sustained FiberJob benchmark did not complete every job");

    size_t executedJobs    = 0;
    size_t minExecutedJobs = TotalJobs;
    size_t maxExecutedJobs = 0;
    size_t claimBatches    = 0;
    size_t claimedJobs     = 0;
    size_t claimBatchPeak  = 0;
    size_t stealAttempts   = 0;
    size_t stolenJobs      = 0;
    size_t failedSteals    = 0;
    for (size_t workerIndex = 0; workerIndex < numWorkers; ++workerIndex)
    {
        FiberJobWorkerDiagnostics diagnostics;
        scheduler.workerDiagnostics(workers[workerIndex], diagnostics);
        executedJobs += diagnostics.executedJobs;
        minExecutedJobs = diagnostics.executedJobs < minExecutedJobs ? diagnostics.executedJobs : minExecutedJobs;
        maxExecutedJobs = diagnostics.executedJobs > maxExecutedJobs ? diagnostics.executedJobs : maxExecutedJobs;
        claimBatches += diagnostics.claimBatches;
        claimedJobs += diagnostics.claimedJobs;
        claimBatchPeak = diagnostics.claimBatchPeak > claimBatchPeak ? diagnostics.claimBatchPeak : claimBatchPeak;
        stealAttempts += diagnostics.stealAttempts;
        stolenJobs += diagnostics.stolenJobs;
        failedSteals += diagnostics.failedSteals;
    }
    SC_TRY_MSG(executedJobs == TotalJobs, "Sustained FiberJob benchmark execution count mismatch");

    const FiberAllocatorStatistics allocatorStatistics = allocator.statistics();
    SC_TRY(scheduler.close());
    SC_TRY(allocator.close());

    const Time::HighResolutionCounter elapsed       = finish.subtractExact(start);
    const int64_t                     elapsedNs     = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const size_t                      jobsPerSecond = static_cast<size_t>(TotalJobs * 1000000000ull / elapsedNs);
    console.print("FibersBenchmark sustained stackless jobs payload={}\n",
                  sharedAtomicPayload ? "shared-atomic" : "independent-record");
    console.print(
        "  workers={} jobs={} batchCapacity={} workIterations={} timing=persistent-batch-publish-through-idle\n",
        numWorkers, TotalJobs, BatchCapacity, static_cast<size_t>(WorkIterations));
    console.print("  elapsedNs={} jobsPerSec={} checksum={} executedJobs={} stolenJobs={}\n",
                  static_cast<size_t>(elapsedNs), jobsPerSecond, checksum, executedJobs, stolenJobs);
    console.print("  workerExecutedMin={} workerExecutedMax={} claimBatches={} claimedJobs={} claimBatchPeak={}\n",
                  minExecutedJobs, maxExecutedJobs, claimBatches, claimedJobs, claimBatchPeak);
    console.print("  stealAttempts={} failedSteals={}\n", stealAttempts, failedSteals);
    console.print("  allocatorPeakBytes={} allocatorFailures={}\n", allocatorStatistics.peakBytesInUse,
                  allocatorStatistics.numAllocationFailures);
    return Result(true);
}

static Result runSustainedFiberJobBenchmark(Console& console, size_t numWorkers)
{
    SC_TRY(runSustainedFiberJobBenchmarkCase(console, numWorkers, true));
    return runSustainedFiberJobBenchmarkCase(console, numWorkers, false);
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
    size_t peerExecutions = 0;
    for (size_t workerIndex = 1; workerIndex < NumWorkers; ++workerIndex)
    {
        FiberWorkerDiagnostics workerDiagnostics;
        scheduler.workerDiagnostics(workers[workerIndex], workerDiagnostics);
        peerExecutions += workerDiagnostics.executedFibers;
    }
    SC_TRY_MSG(peerExecutions > 0, "Forced-steal benchmark did not execute work on a peer worker");
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
    console.print("  peerExecutions={}\n", peerExecutions);
    console.print("  stealAttempts={} stealVictimProbes={} stolenFibers={} stolenBatches={} stolenBatchPeak={} "
                  "failedSteals={} queuePeak={} spilled={}\n",
                  diagnostics.stealAttempts, diagnostics.stealVictimProbes, diagnostics.stolenFibers,
                  diagnostics.stolenBatches, diagnostics.stolenBatchPeak, diagnostics.failedSteals,
                  diagnostics.readyPeakFibers, diagnostics.spilledFibers);
    for (size_t workerIndex = 0; workerIndex < NumWorkers; ++workerIndex)
    {
        FiberWorkerDiagnostics workerDiagnostics;
        scheduler.workerDiagnostics(workers[workerIndex], workerDiagnostics);
        console.print("  worker[{}] queuePeak={} spills={} executed={} completed={} runAttempts={} idlePolls={} "
                      "idleSpins={} parks={} wakes={} stealAttempts={} stealVictimProbes={} steals={} "
                      "stealBatches={} stolenBatchPeak={} failedSteals={}\n",
                      workerIndex, workerDiagnostics.readyPeakFibers, workerDiagnostics.spilledFibers,
                      workerDiagnostics.executedFibers, workerDiagnostics.completedFibers,
                      workerDiagnostics.runAttempts, workerDiagnostics.idlePolls, workerDiagnostics.idleSpinIterations,
                      workerDiagnostics.parkAttempts, workerDiagnostics.parkedWakeups, workerDiagnostics.stealAttempts,
                      workerDiagnostics.stealVictimProbes, workerDiagnostics.stolenFibers,
                      workerDiagnostics.stolenBatches, workerDiagnostics.stolenBatchPeak,
                      workerDiagnostics.failedSteals);
    }

    scheduler.releaseWorkerDeques({workers, NumWorkers});
    SC_TRY(allocator.close());
    return Result(true);
}

[[nodiscard]] static Result runMassSuspensionBenchmark(Console& console, size_t numFibers,
                                                       FiberStackCommitMode commitMode)
{
    static constexpr size_t StackSize     = FiberStackSize::ThirtyTwoKiB;
    const size_t            allocatorSize = numFibers * sizeof(FiberTask) * 2 + 4 * 1024 * 1024;

    FiberStackGrowthRuntime growthRuntime;
    FiberStackGrowthThread  growthThread;
    if (commitMode == FiberStackCommitMode::Incremental)
    {
        SC_TRY_MSG(FiberStackGrowthRuntime::isSupported(),
                   "Incremental mass suspension is unavailable under the active tooling");
        SC_TRY(growthRuntime.create());
#if SC_PLATFORM_WINDOWS
        SC_TRY(growthThread.create(growthRuntime, {}));
#else
        static char signalStack[FiberStackGrowthSignalStackSize] = {};
        SC_TRY(growthThread.create(growthRuntime, signalStack));
#endif
    }

    FiberAllocator               allocator;
    FiberAllocatorVirtualOptions allocatorOptions;
    allocatorOptions.reserveBytes       = allocatorSize;
    allocatorOptions.initialCommitBytes = 64 * 1024;
    SC_TRY(allocator.createVirtual(allocatorOptions));

    FiberTaskClass        taskClass;
    FiberTaskClassOptions taskOptions;
    taskOptions.maxTasks = numFibers;
    SC_TRY(taskClass.create(allocator, taskOptions));

    FiberStackClass        stackClass;
    FiberStackClassOptions stackOptions;
    stackOptions.stackSizeInBytes = StackSize;
    stackOptions.maxStacks        = numFibers;
    stackOptions.guardPage        = true;
    stackOptions.commitMode       = commitMode;
    if (commitMode == FiberStackCommitMode::Incremental)
    {
        stackOptions.initialCommitSizeInBytes = FiberStackSize::FourKiB;
        stackOptions.growthCommitSizeInBytes  = FiberStackSize::FourKiB;
    }
    SC_TRY(stackClass.reserve(stackOptions));

    FiberStackClassDiagnostics emptyDiagnostics;
    stackClass.diagnostics(emptyDiagnostics);

    FiberTaskPool taskPool;
    SC_TRY(taskPool.create(taskClass, stackClass));
    taskPool.fillHighWaterMarks();

    FiberScheduler scheduler;
    FiberCounter   gate;
    scheduler.add(gate);

    Time::HighResolutionCounter spawnStart;
    spawnStart.snap();
    for (size_t fiberIndex = 0; fiberIndex < numFibers; ++fiberIndex)
    {
        SC_TRY(taskPool.spawn(scheduler, FiberTask::Procedure([&gate](FiberScheduler& runningScheduler)
                                                              { return runningScheduler.wait(gate); })));
    }
    Time::HighResolutionCounter spawnFinish;
    spawnFinish.snap();

    Time::HighResolutionCounter suspendStart;
    suspendStart.snap();
    for (size_t fiberIndex = 0; fiberIndex < numFibers; ++fiberIndex)
    {
        SC_TRY(scheduler.runNoWait());
    }
    Time::HighResolutionCounter suspendFinish;
    suspendFinish.snap();

    FiberTaskPoolDiagnostics suspendedDiagnostics;
    taskPool.diagnostics(suspendedDiagnostics);
    SC_TRY_MSG(scheduler.activeFiberCount() == numFibers, "Mass-suspension benchmark did not suspend every fiber");
    SC_TRY_MSG(suspendedDiagnostics.activeTasks == numFibers,
               "Mass-suspension benchmark task class did not retain every task");
    SC_TRY_MSG(suspendedDiagnostics.stackClass.activeStacks == numFibers,
               "Mass-suspension benchmark stack class did not retain every stack");

    Time::HighResolutionCounter wakeStart;
    wakeStart.snap();
    SC_TRY(scheduler.done(gate));
    Time::HighResolutionCounter wakeFinish;
    wakeFinish.snap();

    Time::HighResolutionCounter completionStart;
    completionStart.snap();
    SC_TRY(scheduler.run());
    Time::HighResolutionCounter completionFinish;
    completionFinish.snap();

    FiberTaskPoolDiagnostics completedDiagnostics;
    taskPool.diagnostics(completedDiagnostics);
    SC_TRY_MSG(not scheduler.hasActiveFibers(), "Mass-suspension benchmark left active fibers");
    SC_TRY_MSG(taskPool.availableCount() == numFibers, "Mass-suspension benchmark did not recycle every slot");

    const Time::HighResolutionCounter spawnElapsed      = spawnFinish.subtractExact(spawnStart);
    const Time::HighResolutionCounter suspendElapsed    = suspendFinish.subtractExact(suspendStart);
    const Time::HighResolutionCounter wakeElapsed       = wakeFinish.subtractExact(wakeStart);
    const Time::HighResolutionCounter completionElapsed = completionFinish.subtractExact(completionStart);
    console.print("FibersBenchmark mass suspension milestone\n");
    console.print("  commitMode={} fibers={} stackSize={} guardSize={}\n",
                  commitMode == FiberStackCommitMode::Incremental ? "incremental" : "full", numFibers, StackSize,
                  suspendedDiagnostics.stackClass.guardSizeInBytes);
    console.print("  spawnAndSlotAcquireElapsedMs={} suspendElapsedMs={} wakeElapsedMs={} "
                  "completionAndSlotReleaseElapsedMs={}\n",
                  static_cast<size_t>(spawnElapsed.toMilliseconds().ms),
                  static_cast<size_t>(suspendElapsed.toMilliseconds().ms),
                  static_cast<size_t>(wakeElapsed.toMilliseconds().ms),
                  static_cast<size_t>(completionElapsed.toMilliseconds().ms));
    console.print("  reservedBytes={} metadataCommittedBytes={} initialCommitBytes={} growthCommitBytes={}\n",
                  suspendedDiagnostics.stackClass.reservedSizeBytes, emptyDiagnostics.committedSizeBytes,
                  suspendedDiagnostics.stackClass.initialCommitSizeInBytes,
                  suspendedDiagnostics.stackClass.growthCommitSizeInBytes);
    console.print("  suspendedCommittedBytes={} suspendedSlotCommittedBytes={} peakCommittedBytes={}\n",
                  suspendedDiagnostics.stackClass.committedSizeBytes,
                  suspendedDiagnostics.stackClass.committedSizeBytes - emptyDiagnostics.committedSizeBytes,
                  suspendedDiagnostics.stackClass.peakCommittedBytes);
    console.print("  completedCommittedBytes={} taskAllocatorPeakBytes={} allocatorFailures={}\n",
                  completedDiagnostics.stackClass.committedSizeBytes, allocator.peakUsed(),
                  allocator.statistics().numAllocationFailures);
    console.print("  peakStackHighWaterUsed={}\n", completedDiagnostics.stackClass.highWaterUsedBytes);

    SC_TRY(taskPool.close());
    SC_TRY(taskClass.close());
    stackClass.release();
    SC_TRY(allocator.close());
    if (commitMode == FiberStackCommitMode::Incremental)
    {
        SC_TRY(growthThread.close());
        SC_TRY(growthRuntime.close());
    }
    return Result(true);
}

[[nodiscard]] static Result runAsyncFiberHighWaterBenchmark(Console& console)
{
    static constexpr size_t NumTasks  = 64;
    static constexpr size_t StackSize = FiberStackSize::SixtyFourKiB;

    struct State
    {
        AsyncFiberIO* io        = nullptr;
        int           completed = 0;
    };

    AsyncEventLoop eventLoop;
    SC_TRY(eventLoop.create());

    FiberScheduler scheduler;
    AsyncFiberIO   io(scheduler, eventLoop);
    FiberTask      tasks[NumTasks];
    static char    stackMemory[NumTasks * StackSize] = {};
    FiberTaskPool  taskPool({tasks, NumTasks}, {stackMemory, sizeof(stackMemory)}, StackSize);
    State          state;
    state.io = &io;

    taskPool.fillHighWaterMarks();
    for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
    {
        SC_TRY(taskPool.spawn(scheduler, FiberTask::Procedure(
                                             [&state](FiberScheduler&)
                                             {
                                                 SC_TRY(state.io->sleep(TimeMs{1}));
                                                 state.completed += 1;
                                                 return Result(true);
                                             })));
    }

    Time::HighResolutionCounter start;
    start.snap();
    SC_TRY(io.run());
    Time::HighResolutionCounter finish;
    finish.snap();

    SC_TRY_MSG(state.completed == static_cast<int>(NumTasks), "AsyncFibers high-water benchmark did not complete");

    size_t maxStackUsed = 0;
    for (size_t taskIndex = 0; taskIndex < taskPool.capacity(); ++taskIndex)
    {
        size_t stackUsed = 0;
        SC_TRY(taskPool.stackHighWaterUsedBytes(taskIndex, stackUsed));
        if (stackUsed > maxStackUsed)
        {
            maxStackUsed = stackUsed;
        }
    }

    const Time::HighResolutionCounter elapsed = finish.subtractExact(start);
    console.print("FibersBenchmark AsyncFibers stack high water\n");
    console.print("  tasks={} stackSize={} elapsedMs={} maxStackUsed={}\n", NumTasks, StackSize,
                  static_cast<size_t>(elapsed.toMilliseconds().ms), maxStackUsed);

    SC_TRY(eventLoop.close());
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

    Time::HighResolutionCounter producerStarted;
    Time::HighResolutionCounter producerFinished;
    int                         workIterations = 0;
};

struct MicroTaskPhaseMetrics
{
    int64_t producerElapsedNs   = 0;
    int64_t postProducerDrainNs = 0;
    bool    hasProducerTiming   = false;
};

struct MicroTaskExternalProducerState
{
    FiberScheduler*          scheduler      = nullptr;
    MicroTaskBenchmarkState* benchmarkState = nullptr;
    FiberEvent*              producerDone   = nullptr;
    Atomic<int32_t>*         producersLeft  = nullptr;
    Semaphore*               startGate      = nullptr;
    FiberTask*               tasks          = nullptr;
    char*                    stackMemory    = nullptr;
    size_t                   numJobs        = 0;
    size_t                   stackSize      = 0;
    Result                   producerResult = Result(true);

    Time::HighResolutionCounter producerStarted;
    Time::HighResolutionCounter producerFinished;
};

static Result runCpuPayload(MicroTaskBenchmarkState& state, int workIterations)
{
    // The seed is runtime state so a compiler cannot reduce the payload to a constant expression.
    uint32_t value = static_cast<uint32_t>(state.completed.load(memory_order_relaxed)) + 1;
    for (int idx = 0; idx < workIterations; ++idx)
    {
        value = (value * 1664525u) ^ static_cast<uint32_t>(idx + 1013904223u);
    }
    state.checksum.fetch_add(static_cast<int32_t>(value), memory_order_relaxed);
    state.completed.fetch_add(1, memory_order_relaxed);
    return Result(true);
}

static Result spawnMicroTaskJob(FiberTaskPool& pool, FiberScheduler& scheduler, MicroTaskBenchmarkState& state,
                                int workIterations)
{
    SC_TRY(pool.spawn(scheduler, FiberTask::Procedure([&state, workIterations](FiberScheduler&)
                                                      { return runCpuPayload(state, workIterations); })));
    state.submitted.fetch_add(1, memory_order_relaxed);
    return Result(true);
}

static Result spawnExternalMicroTaskJob(FiberTask& task, Span<char> stackMemory, FiberScheduler& scheduler,
                                        MicroTaskBenchmarkState& state, int workIterations)
{
    FiberStack stack(stackMemory);
    SC_TRY(scheduler.spawn(task, stack,
                           FiberTask::Procedure([&state, workIterations](FiberScheduler&)
                                                { return runCpuPayload(state, workIterations); })));
    state.submitted.fetch_add(1, memory_order_relaxed);
    return Result(true);
}

static Result printMicroTaskMetrics(Console& console, MicroTaskProducerMode mode, size_t numWorkers,
                                    size_t numExternalProducers, size_t numJobs, int workIterations,
                                    size_t configuredInjectionCapacity, const Time::HighResolutionCounter& elapsed,
                                    const FiberScheduler& scheduler, const FiberWorkerPool& workerPool,
                                    Span<FiberWorker> workers, const FiberAllocatorStatistics& allocatorStatistics,
                                    const MicroTaskBenchmarkState& state, const MicroTaskPhaseMetrics& phaseMetrics)
{
    const int64_t elapsedNs  = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const int64_t elapsedMs  = elapsed.toMilliseconds().ms;
    const int64_t completed  = state.completed.load(memory_order_relaxed);
    const int64_t jobsPerSec = completed * 1000000000 / elapsedNs;

    FiberWorkerDiagnostics aggregateWorkerDiagnostics;
    scheduler.workerDiagnostics(workers, aggregateWorkerDiagnostics);

    FiberSchedulerDiagnostics schedulerDiagnostics;
    scheduler.schedulerDiagnostics(schedulerDiagnostics);

    FiberWorkerPoolWakeDiagnostics wakeDiagnostics;
    workerPool.wakeDiagnostics(wakeDiagnostics);

    console.print("FibersBenchmark micro-tasking: mode={} workers={} externalProducers={} jobs={}\n",
                  microTaskProducerModeName(mode), numWorkers, numExternalProducers, numJobs);
    console.print("  workIterations={} elapsedMs={} elapsedNs={}\n", static_cast<size_t>(workIterations),
                  static_cast<size_t>(elapsedMs), static_cast<size_t>(elapsedNs));
    if (phaseMetrics.hasProducerTiming)
    {
        console.print("  producerElapsedNs={} postProducerDrainNs={}\n",
                      static_cast<size_t>(phaseMetrics.producerElapsedNs),
                      static_cast<size_t>(phaseMetrics.postProducerDrainNs));
    }
    console.print("  submitted={} completed={} jobsPerSec={} checksum={}\n",
                  static_cast<size_t>(state.submitted.load(memory_order_relaxed)), static_cast<size_t>(completed),
                  static_cast<size_t>(jobsPerSec), static_cast<size_t>(state.checksum.load(memory_order_relaxed)));
    console.print(
        "  runAttempts={} idlePolls={} idleSpinIterations={} parkAttempts={} parkedWakeups={} executedFibers={} "
        "completedFibers={}\n",
        aggregateWorkerDiagnostics.runAttempts, aggregateWorkerDiagnostics.idlePolls,
        aggregateWorkerDiagnostics.idleSpinIterations, aggregateWorkerDiagnostics.parkAttempts,
        aggregateWorkerDiagnostics.parkedWakeups, aggregateWorkerDiagnostics.executedFibers,
        aggregateWorkerDiagnostics.completedFibers);
    console.print("  stealAttempts={} stealVictimProbes={} stolenFibers={} stolenBatches={} stolenBatchPeak={} "
                  "failedSteals={}\n",
                  aggregateWorkerDiagnostics.stealAttempts, aggregateWorkerDiagnostics.stealVictimProbes,
                  aggregateWorkerDiagnostics.stolenFibers, aggregateWorkerDiagnostics.stolenBatches,
                  aggregateWorkerDiagnostics.stolenBatchPeak, aggregateWorkerDiagnostics.failedSteals);
    console.print("  queuePeak={} spilled={}\n", aggregateWorkerDiagnostics.readyPeakFibers,
                  aggregateWorkerDiagnostics.spilledFibers);
    console.print("  schedulerLockAcquisitions={} schedulerLockContentions={} schedulerLockSpinRetries={}\n",
                  schedulerDiagnostics.lockAcquisitions, schedulerDiagnostics.lockContentions,
                  schedulerDiagnostics.lockSpinRetries);
    console.print("  schedulerLocksByCategory: spawn={} ready={} synchronization={} completion={} control={}\n",
                  schedulerDiagnostics.lockSpawn, schedulerDiagnostics.lockReady,
                  schedulerDiagnostics.lockSynchronization, schedulerDiagnostics.lockCompletion,
                  schedulerDiagnostics.lockControl);
    console.print("  configuredInjectionCapacity={} injectionPeak={} injectionSpills={} injectionClaimBatchPeak={}\n",
                  configuredInjectionCapacity, schedulerDiagnostics.injectionPeak, schedulerDiagnostics.injectionSpills,
                  schedulerDiagnostics.injectionClaimBatchPeak);
    console.print("  injectionLockAcquisitions={} injectionLockContentions={} injectionLockSpinRetries={}\n",
                  schedulerDiagnostics.injectionLockAcquisitions, schedulerDiagnostics.injectionLockContentions,
                  schedulerDiagnostics.injectionLockSpinRetries);
    console.print("  allocatorPeakBytes={} allocatorFailures={}\n", allocatorStatistics.peakBytesInUse,
                  allocatorStatistics.numAllocationFailures);
    console.print("  wakeNotifications={} wakeSignals={}\n", wakeDiagnostics.wakeNotifications,
                  wakeDiagnostics.wakeSignals);
    for (size_t workerIndex = 0; workerIndex < workers.sizeInElements(); ++workerIndex)
    {
        FiberWorkerDiagnostics workerDiagnostics;
        scheduler.workerDiagnostics(workers[workerIndex], workerDiagnostics);
        console.print("  worker[{}] queuePeak={} spills={} executed={} completed={} runAttempts={} idlePolls={} "
                      "idleSpins={} parks={} wakes={} stealAttempts={} stealVictimProbes={} steals={} "
                      "stealBatches={} stolenBatchPeak={} failedSteals={}\n",
                      workerIndex, workerDiagnostics.readyPeakFibers, workerDiagnostics.spilledFibers,
                      workerDiagnostics.executedFibers, workerDiagnostics.completedFibers,
                      workerDiagnostics.runAttempts, workerDiagnostics.idlePolls, workerDiagnostics.idleSpinIterations,
                      workerDiagnostics.parkAttempts, workerDiagnostics.parkedWakeups, workerDiagnostics.stealAttempts,
                      workerDiagnostics.stealVictimProbes, workerDiagnostics.stolenFibers,
                      workerDiagnostics.stolenBatches, workerDiagnostics.stolenBatchPeak,
                      workerDiagnostics.failedSteals);
    }
    return Result(true);
}

static Result runMicroTaskBenchmarkCase(Console& console, MicroTaskProducerMode mode, size_t numWorkers,
                                        size_t numExternalProducers, int workIterations)
{
    static constexpr size_t MaxWorkers             = 16;
    static constexpr size_t MaxExternalProducers   = 8;
    static constexpr size_t NumJobs                = 8192;
    static constexpr size_t InFiberPoolCapacity    = 256;
    static constexpr size_t StackSize              = 32 * 1024;
    static constexpr size_t DequeCapacityPerWorker = 256;
    static constexpr size_t InjectionCapacity      = NumJobs + 1;
    SC_TRY_MSG(numWorkers > 0 and numWorkers <= MaxWorkers, "Invalid micro-task worker count");
    SC_TRY_MSG(numExternalProducers > 0 and numExternalProducers <= MaxExternalProducers,
               "Invalid micro-task external producer count");

    FiberScheduler    scheduler;
    FiberWorker       workers[MaxWorkers];
    FiberWorkerThread threads[MaxWorkers];
    FiberWorkerPool   workerPool;

    FiberAllocator allocator;

    static char schedulerStorage[MaxWorkers * DequeCapacityPerWorker * sizeof(FiberTask*) +
                                 InjectionCapacity * FiberInjectionSlotStorageSize + 4096] = {};

    FiberWorkerPoolOptions workerPoolOptions;
    workerPoolOptions.dequeAllocator         = &allocator;
    workerPoolOptions.dequeCapacityPerWorker = DequeCapacityPerWorker;
    workerPoolOptions.injectionAllocator     = &allocator;
    workerPoolOptions.injectionCapacity      = InjectionCapacity;

    MicroTaskBenchmarkState state;
    state.workIterations = workIterations;
    SC_TRY(allocator.createFixed(schedulerStorage));

    Time::HighResolutionCounter start;
    Time::HighResolutionCounter finish;
    MicroTaskPhaseMetrics       phaseMetrics;

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
                    state.producerStarted.snap();
                    while (state.submitted.load(memory_order_relaxed) < static_cast<int32_t>(NumJobs))
                    {
                        Result spawnResult = spawnMicroTaskJob(taskPool, scheduler, state, state.workIterations);
                        if (spawnResult)
                        {
                            continue;
                        }
                        SC_TRY(taskPool.waitForAvailableTask(scheduler));
                    }
                    state.producerFinished.snap();
                    state.producerDone.store(true, memory_order_release);
                    return Result(true);
                })));

        start.snap();
        SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, workerPoolOptions));
        SC_TRY(workerPool.join());
        finish.snap();
        phaseMetrics.hasProducerTiming = true;
        phaseMetrics.producerElapsedNs = state.producerFinished.subtractExact(state.producerStarted).toNanoseconds().ns;
        phaseMetrics.postProducerDrainNs = finish.subtractExact(state.producerFinished).toNanoseconds().ns;
    }
    else
    {
        static FiberTask tasks[NumJobs];
        static char      stackMemory[NumJobs * StackSize] = {};
        FiberTaskPool    taskPool({tasks, NumJobs}, {stackMemory, sizeof(stackMemory)}, StackSize);

        if (mode == MicroTaskProducerMode::ExternalBeforeWorkers)
        {
            start.snap();
            state.producerStarted.snap();
            for (size_t idx = 0; idx < NumJobs; ++idx)
            {
                SC_TRY(spawnMicroTaskJob(taskPool, scheduler, state, workIterations));
            }
            state.producerFinished.snap();
            state.producerDone.store(true, memory_order_release);
            SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, workerPoolOptions));
            SC_TRY(workerPool.join());
            finish.snap();
            phaseMetrics.hasProducerTiming = true;
            phaseMetrics.producerElapsedNs =
                state.producerFinished.subtractExact(state.producerStarted).toNanoseconds().ns;
            phaseMetrics.postProducerDrainNs = finish.subtractExact(state.producerFinished).toNanoseconds().ns;
        }
        else
        {
            FiberTask  keepAliveTask;
            char       keepAliveStackMemory[StackSize] = {};
            FiberStack keepAliveStack({keepAliveStackMemory, sizeof(keepAliveStackMemory)});
            FiberEvent producerDone;

            SC_TRY(scheduler.spawn(keepAliveTask, keepAliveStack,
                                   FiberTask::Procedure([&producerDone](FiberScheduler& scheduler)
                                                        { return producerDone.wait(scheduler); })));
            SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, workerPoolOptions));

            Thread          producerThreads[MaxExternalProducers];
            Atomic<int32_t> producersLeft(static_cast<int32_t>(numExternalProducers));
            Semaphore       producerStartGate;
            start.snap();
            MicroTaskExternalProducerState producerStates[MaxExternalProducers];
            size_t                         firstJob            = 0;
            size_t                         numStartedProducers = 0;
            Result                         producerStartResult = Result(true);
            for (size_t producerIndex = 0; producerIndex < numExternalProducers; ++producerIndex)
            {
                const size_t numProducerJobs =
                    NumJobs / numExternalProducers + (producerIndex < NumJobs % numExternalProducers ? 1 : 0);
                MicroTaskExternalProducerState& producerState = producerStates[producerIndex];
                producerState.scheduler                       = &scheduler;
                producerState.benchmarkState                  = &state;
                producerState.producerDone                    = &producerDone;
                producerState.producersLeft                   = &producersLeft;
                producerState.startGate                       = &producerStartGate;
                producerState.tasks                           = tasks + firstJob;
                producerState.stackMemory                     = stackMemory + firstJob * StackSize;
                producerState.numJobs                         = numProducerJobs;
                producerState.stackSize                       = StackSize;
                firstJob += numProducerJobs;

                Result startResult = producerThreads[producerIndex].start(
                    [&producerState](Thread&)
                    {
                        producerState.startGate->acquire();
                        producerState.producerStarted.snap();
                        for (size_t idx = 0; idx < producerState.numJobs; ++idx)
                        {
                            Result result = spawnExternalMicroTaskJob(
                                producerState.tasks[idx],
                                {producerState.stackMemory + idx * producerState.stackSize, producerState.stackSize},
                                *producerState.scheduler, *producerState.benchmarkState,
                                producerState.benchmarkState->workIterations);
                            if (not result)
                            {
                                producerState.producerResult = result;
                                break;
                            }
                        }
                        producerState.producerFinished.snap();
                        if (producerState.producersLeft->fetch_sub(1, memory_order_acq_rel) == 1)
                        {
                            producerState.benchmarkState->producerDone.store(true, memory_order_release);
                            Result signalResult = producerState.producerDone->signal(*producerState.scheduler);
                            if (producerState.producerResult and not signalResult)
                            {
                                producerState.producerResult = signalResult;
                            }
                        }
                    });
                if (not startResult)
                {
                    producerStartResult = startResult;
                    break;
                }
                numStartedProducers += 1;
            }
            producersLeft.store(static_cast<int32_t>(numStartedProducers), memory_order_release);
            if (numStartedProducers == 0)
            {
                SC_TRY(producerDone.signal(scheduler));
            }
            for (size_t producerIndex = 0; producerIndex < numStartedProducers; ++producerIndex)
            {
                producerStartGate.release();
            }
            Result producerJoinResult = Result(true);
            Result producerRunResult  = Result(true);
            for (size_t producerIndex = 0; producerIndex < numStartedProducers; ++producerIndex)
            {
                Result joinResult = producerThreads[producerIndex].join();
                if (producerJoinResult and not joinResult)
                {
                    producerJoinResult = joinResult;
                }
                if (producerRunResult and not producerStates[producerIndex].producerResult)
                {
                    producerRunResult = producerStates[producerIndex].producerResult;
                }
            }
            SC_TRY(workerPool.join());
            SC_TRY(producerJoinResult);
            SC_TRY(producerRunResult);
            SC_TRY(producerStartResult);
            SC_TRY_MSG(firstJob == NumJobs, "Micro-task producer ranges did not cover all jobs");
            finish.snap();

            if (numStartedProducers > 0)
            {
                Time::HighResolutionCounter firstProducerStart = producerStates[0].producerStarted;
                Time::HighResolutionCounter lastProducerFinish = producerStates[0].producerFinished;
                for (size_t producerIndex = 1; producerIndex < numStartedProducers; ++producerIndex)
                {
                    if (firstProducerStart.isLaterThanOrEqualTo(producerStates[producerIndex].producerStarted))
                    {
                        firstProducerStart = producerStates[producerIndex].producerStarted;
                    }
                    if (producerStates[producerIndex].producerFinished.isLaterThanOrEqualTo(lastProducerFinish))
                    {
                        lastProducerFinish = producerStates[producerIndex].producerFinished;
                    }
                }
                phaseMetrics.hasProducerTiming = true;
                phaseMetrics.producerElapsedNs =
                    lastProducerFinish.subtractExact(firstProducerStart).toNanoseconds().ns;
                phaseMetrics.postProducerDrainNs = finish.subtractExact(lastProducerFinish).toNanoseconds().ns;
            }
        }
    }

    SC_TRY_MSG(state.submitted.load(memory_order_relaxed) == static_cast<int32_t>(NumJobs),
               "Micro-task benchmark did not submit all jobs");
    SC_TRY_MSG(state.completed.load(memory_order_relaxed) == static_cast<int32_t>(NumJobs),
               "Micro-task benchmark did not complete all jobs");

    const FiberAllocatorStatistics allocatorStatistics = allocator.statistics();
    SC_TRY(allocator.close());

    return printMicroTaskMetrics(console, mode, numWorkers, numExternalProducers, NumJobs, workIterations,
                                 InjectionCapacity, finish.subtractExact(start), scheduler, workerPool,
                                 {workers, numWorkers}, allocatorStatistics, state, phaseMetrics);
}

static Result runMicroTaskBenchmarks(Console& console, size_t numExternalProducers, size_t selectedWorkers,
                                     StringView selectedWorkload)
{
    static constexpr size_t MaxBenchmarkCounts = 5;
    static constexpr size_t MaxWorkers         = 16;
    static constexpr int    TinyWorkIterations = 16;
    static constexpr int    CpuWorkIterations  = 20000;

    size_t workerCountsStorage[MaxBenchmarkCounts] = {};
    size_t numWorkerCounts                         = 0;

    if (selectedWorkers > 0)
    {
        workerCountsStorage[numWorkerCounts++] = selectedWorkers;
    }
    else
    {
        const size_t requestedCounts[] = {1, 2, 4, 8, availableHardwareWorkers()};
        for (size_t requestedCount : requestedCounts)
        {
            SC_TRY_MSG(requestedCount <= MaxWorkers,
                       "FibersBenchmark must increase its fixed worker storage for this machine");
            size_t workerCount = requestedCount;
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
    }

    const MicroTaskProducerMode modes[] = {
        MicroTaskProducerMode::ExternalBeforeWorkers,
        MicroTaskProducerMode::ExternalWhileWorkersRunning,
        MicroTaskProducerMode::InFiberProducer,
    };

    for (MicroTaskProducerMode mode : modes)
    {
        const bool selected =
            selectedWorkload == "all" or
            (selectedWorkload == "preloaded" and mode == MicroTaskProducerMode::ExternalBeforeWorkers) or
            (selectedWorkload == "external" and mode == MicroTaskProducerMode::ExternalWhileWorkersRunning) or
            (selectedWorkload == "in-fiber" and mode == MicroTaskProducerMode::InFiberProducer);
        if (not selected)
        {
            continue;
        }
        for (size_t idx = 0; idx < numWorkerCounts; ++idx)
        {
            const size_t producers =
                mode == MicroTaskProducerMode::ExternalWhileWorkersRunning ? numExternalProducers : 1;
            SC_TRY(runMicroTaskBenchmarkCase(console, mode, workerCountsStorage[idx], producers, TinyWorkIterations));
        }
    }

    if (selectedWorkload == "all" or selectedWorkload == "balanced")
    {
        console.print("FibersBenchmark balanced CPU payload\n");
        for (size_t idx = 0; idx < numWorkerCounts; ++idx)
        {
            SC_TRY(runMicroTaskBenchmarkCase(console, MicroTaskProducerMode::ExternalBeforeWorkers,
                                             workerCountsStorage[idx], 1, CpuWorkIterations));
        }
    }
    return Result(true);
}

static Result runSustainedMicroTaskBenchmark(Console& console, size_t selectedWorkers)
{
    static constexpr size_t MaxWorkers             = 16;
    static constexpr size_t NumJobs                = 1000000;
    static constexpr size_t PoolCapacity           = 512;
    static constexpr size_t StackSize              = 32 * 1024;
    static constexpr size_t DequeCapacityPerWorker = 256;
    static constexpr size_t InjectionCapacity      = PoolCapacity + 1;
    static constexpr size_t AvailabilityBatch      = 64;
    static constexpr int    WorkIterations         = 4;

    size_t numWorkers = selectedWorkers > 0 ? selectedWorkers : availableHardwareWorkers();
    if (selectedWorkers == 0 and numWorkers > 8)
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

    static char schedulerStorage[MaxWorkers * DequeCapacityPerWorker * sizeof(FiberTask*) +
                                 InjectionCapacity * FiberInjectionSlotStorageSize + 4096] = {};

    FiberWorkerPoolOptions workerPoolOptions;
    workerPoolOptions.dequeAllocator         = &allocator;
    workerPoolOptions.dequeCapacityPerWorker = DequeCapacityPerWorker;
    workerPoolOptions.injectionAllocator     = &allocator;
    workerPoolOptions.injectionCapacity      = InjectionCapacity;

    static FiberTask tasks[PoolCapacity];
    static char      stackMemory[PoolCapacity * StackSize] = {};
    FiberTaskPool    taskPool({tasks, PoolCapacity}, {stackMemory, sizeof(stackMemory)}, StackSize);

    FiberTask  producerTask;
    char       producerStackMemory[StackSize] = {};
    FiberStack producerStack({producerStackMemory, sizeof(producerStackMemory)});

    MicroTaskBenchmarkState state;
    SC_TRY(allocator.createFixed(schedulerStorage));

    SC_TRY(scheduler.spawn(producerTask, producerStack,
                           FiberTask::Procedure(
                               [&state, &taskPool](FiberScheduler& scheduler)
                               {
                                   state.producerStarted.snap();
                                   while (state.submitted.load(memory_order_relaxed) < static_cast<int32_t>(NumJobs))
                                   {
                                       Result spawnResult =
                                           spawnMicroTaskJob(taskPool, scheduler, state, WorkIterations);
                                       if (spawnResult)
                                       {
                                           continue;
                                       }
                                       SC_TRY(taskPool.waitForAvailableTasks(scheduler, AvailabilityBatch));
                                   }
                                   state.producerFinished.snap();
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

    MicroTaskPhaseMetrics phaseMetrics;
    phaseMetrics.hasProducerTiming   = true;
    phaseMetrics.producerElapsedNs   = state.producerFinished.subtractExact(state.producerStarted).toNanoseconds().ns;
    phaseMetrics.postProducerDrainNs = finish.subtractExact(state.producerFinished).toNanoseconds().ns;

    console.print("FibersBenchmark sustained micro-tasking\n");
    console.print("  poolCapacity={} dequeCapacityPerWorker={}\n", PoolCapacity, DequeCapacityPerWorker);
    return printMicroTaskMetrics(console, MicroTaskProducerMode::InFiberProducer, numWorkers, 1, NumJobs,
                                 WorkIterations, InjectionCapacity, finish.subtractExact(start), scheduler, workerPool,
                                 {workers, numWorkers}, allocatorStatistics, state, phaseMetrics);
}

static Result runCounterCompletionBenchmark(Console& console)
{
    static constexpr size_t MaxWorkers             = 16;
    static constexpr size_t NumTasks               = 1024;
    static constexpr size_t StackSize              = 32 * 1024;
    static constexpr size_t DequeCapacityPerWorker = 256;
    static constexpr size_t InjectionCapacity      = NumTasks + 1;

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
    FiberCounter      completionCounter;
    Atomic<int32_t>   completed;

    static FiberTask tasks[NumTasks];
    static char      stackMemory[NumTasks * StackSize] = {};

    FiberAllocator allocator;

    static char schedulerStorage[MaxWorkers * DequeCapacityPerWorker * sizeof(FiberTask*) +
                                 InjectionCapacity * FiberInjectionSlotStorageSize + 4096] = {};

    FiberWorkerPoolOptions workerPoolOptions;
    workerPoolOptions.dequeAllocator         = &allocator;
    workerPoolOptions.dequeCapacityPerWorker = DequeCapacityPerWorker;
    workerPoolOptions.injectionAllocator     = &allocator;
    workerPoolOptions.injectionCapacity      = InjectionCapacity;

    SC_TRY(allocator.createFixed(schedulerStorage));
    for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
    {
        FiberStack stack({stackMemory + taskIndex * StackSize, StackSize});
        SC_TRY(scheduler.spawn(tasks[taskIndex], stack,
                               FiberTask::Procedure(
                                   [&completed](FiberScheduler&)
                                   {
                                       completed.fetch_add(1, memory_order_relaxed);
                                       return Result(true);
                                   }),
                               &completionCounter));
    }

    scheduler.resetSchedulerDiagnostics();
    Time::HighResolutionCounter start;
    start.snap();
    SC_TRY(workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, workerPoolOptions));
    SC_TRY(workerPool.join());
    Time::HighResolutionCounter finish;
    finish.snap();

    SC_TRY_MSG(completed.load(memory_order_relaxed) == static_cast<int32_t>(NumTasks),
               "Counter completion benchmark did not complete all tasks");
    SC_TRY_MSG(completionCounter.value() == 0, "Counter completion benchmark counter did not reach zero");
    SC_TRY_MSG(not scheduler.hasActiveFibers(), "Counter completion benchmark retained active tasks");

    FiberSchedulerDiagnostics schedulerDiagnostics;
    scheduler.schedulerDiagnostics(schedulerDiagnostics);
    FiberWorkerDiagnostics workerDiagnostics;
    scheduler.workerDiagnostics({workers, numWorkers}, workerDiagnostics);

    const Time::HighResolutionCounter elapsed        = finish.subtractExact(start);
    const int64_t                     elapsedNs      = elapsed.toNanoseconds().ns > 0 ? elapsed.toNanoseconds().ns : 1;
    const int64_t                     tasksPerSecond = static_cast<int64_t>(NumTasks) * 1000000000 / elapsedNs;
    console.print("FibersBenchmark counter-backed completion: workers={} tasks={}\n", numWorkers, NumTasks);
    console.print("  elapsedNs={} tasksPerSec={} completed={} counter={}\n", static_cast<size_t>(elapsedNs),
                  static_cast<size_t>(tasksPerSecond), static_cast<size_t>(completed.load(memory_order_relaxed)),
                  completionCounter.value());
    console.print("  schedulerLockAcquisitions={} schedulerLockContentions={} schedulerLockSpinRetries={}\n",
                  schedulerDiagnostics.lockAcquisitions, schedulerDiagnostics.lockContentions,
                  schedulerDiagnostics.lockSpinRetries);
    console.print("  schedulerLocksByCategory: spawn={} ready={} synchronization={} completion={} control={}\n",
                  schedulerDiagnostics.lockSpawn, schedulerDiagnostics.lockReady,
                  schedulerDiagnostics.lockSynchronization, schedulerDiagnostics.lockCompletion,
                  schedulerDiagnostics.lockControl);
    console.print("  executedFibers={} completedFibers={} stolenFibers={}\n", workerDiagnostics.executedFibers,
                  workerDiagnostics.completedFibers, workerDiagnostics.stolenFibers);

    SC_TRY(allocator.close());
    return Result(true);
}

static Result runFibersBenchmark(int argc, const char* const* argv)
{
    Console console;
    Console::tryAttachingToParentConsole();
    printBenchmarkEnvironment(console);

    bool       schedulerThroughput  = false;
    bool       jobThroughput        = false;
    bool       jobPoolThroughput    = false;
    bool       jobWorkerThroughput  = false;
    bool       jobWorkerMatrix      = false;
    bool       jobWorkerSustained   = false;
    int32_t    jobWorkers           = 4;
    int32_t    jobRounds            = 5;
    int32_t    massSuspensionCount  = 0;
    int32_t    externalProducers    = 1;
    int32_t    schedulerWorkers     = 0;
    int32_t    schedulerRounds      = 1;
    StringView massSuspensionCommit = "full";
    StringView schedulerWorkload    = "all";

    CommandLineOption options[14];
    options[0].longName = "scheduler-throughput";
    options[0].help     = "Run scheduler throughput workloads without density or I/O cases";
    options[0].value    = CommandLineValue::boolean(schedulerThroughput);

    options[1].longName = "job-throughput";
    options[1].help     = "Run the single-thread-driven stackless FiberJob workload";
    options[1].value    = CommandLineValue::boolean(jobThroughput);

    options[2].longName = "job-pool-throughput";
    options[2].help     = "Run stackless FiberJob acquire, execute, retain, and release batches";
    options[2].value    = CommandLineValue::boolean(jobPoolThroughput);

    options[3].longName = "job-worker-throughput";
    options[3].help     = "Run preloaded stackless FiberJobs on the OS-thread worker pool";
    options[3].value    = CommandLineValue::boolean(jobWorkerThroughput);

    options[4].longName  = "job-workers";
    options[4].help      = "Set the worker count for the stackless FiberJob worker-pool workload";
    options[4].valueName = "COUNT";
    options[4].value     = CommandLineValue::int32(jobWorkers);

    options[5].longName = "job-worker-matrix";
    options[5].help     = "Run repeated stackless FiberJob samples with 1, 2, 4, 8, and all hardware workers";
    options[5].value    = CommandLineValue::boolean(jobWorkerMatrix);

    options[6].longName  = "job-rounds";
    options[6].help      = "Set measured rounds per worker count for the stackless FiberJob matrix";
    options[6].valueName = "COUNT";
    options[6].value     = CommandLineValue::int32(jobRounds);

    options[7].longName = "job-worker-sustained";
    options[7].help     = "Run reusable stackless FiberJob waves on one persistent worker pool";
    options[7].value    = CommandLineValue::boolean(jobWorkerSustained);

    options[8].longName  = "mass-suspension";
    options[8].help      = "Run one mass-suspension workload with the requested live fiber count";
    options[8].valueName = "COUNT";
    options[8].value     = CommandLineValue::int32(massSuspensionCount);

    options[9].longName  = "external-producers";
    options[9].help      = "Use concurrent external producers in scheduler throughput workloads";
    options[9].valueName = "COUNT";
    options[9].value     = CommandLineValue::int32(externalProducers);

    options[10].longName  = "scheduler-workers";
    options[10].help      = "Run scheduler micro-task workloads at one worker count instead of the default matrix";
    options[10].valueName = "COUNT";
    options[10].value     = CommandLineValue::int32(schedulerWorkers);

    options[11].longName = "scheduler-workload";
    options[11].help =
        "Select all, worker-pool, forced-steal, preloaded, external, in-fiber, balanced, counter, or sustained";
    options[11].valueName = "NAME";
    options[11].value     = CommandLineValue::stringView(schedulerWorkload);

    options[12].longName  = "scheduler-rounds";
    options[12].help      = "Repeat the selected scheduler workload for profiling";
    options[12].valueName = "COUNT";
    options[12].value     = CommandLineValue::int32(schedulerRounds);

    options[13].longName  = "mass-suspension-commit";
    options[13].help      = "Select full or incremental stack commitment for mass suspension";
    options[13].valueName = "MODE";
    options[13].value     = CommandLineValue::stringView(massSuspensionCommit);

    CommandLineSpec spec;
    spec.programName = "FibersBenchmark";
    spec.summary     = "Measure Fibers scheduler throughput, contention, and live-fiber density.";
    spec.options     = options;

    StringSpan           argumentStorage[16];
    CommandLineArguments arguments;
    SC_TRY(arguments.setFromMainArguments(argc, argv, argumentStorage));
    const CommandLineParseResult parseResult = spec.parse(arguments.values);
    if (parseResult.status == CommandLineParseResult::Status::HelpRequested)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, true);
        SC_TRY_MSG(spec.writeHelp(output), "Failed writing FibersBenchmark help");
        console.flush();
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, false);
        SC_TRY_MSG(spec.writeError(parseResult, output), "Failed writing FibersBenchmark parse error");
        console.flushStdErr();
        return Result::Error("Invalid FibersBenchmark arguments");
    }
    const size_t selectedModes = static_cast<size_t>(schedulerThroughput) + static_cast<size_t>(jobThroughput) +
                                 static_cast<size_t>(jobPoolThroughput) + static_cast<size_t>(jobWorkerThroughput) +
                                 static_cast<size_t>(jobWorkerMatrix) + static_cast<size_t>(jobWorkerSustained) +
                                 static_cast<size_t>(massSuspensionCount != 0);
    if (selectedModes > 1)
    {
        return Result::Error("Throughput and mass-suspension modes are mutually exclusive");
    }
    if (externalProducers <= 0 or externalProducers > 8)
    {
        return Result::Error("External producer count must be between one and eight");
    }
    if (schedulerWorkers < 0 or schedulerWorkers > 16)
    {
        return Result::Error("Scheduler worker count must be between one and 16");
    }
    if (schedulerRounds <= 0 or schedulerRounds > 1000)
    {
        return Result::Error("Scheduler rounds must be between one and 1000");
    }
    if (schedulerWorkload != "all" and schedulerWorkload != "worker-pool" and schedulerWorkload != "forced-steal" and
        schedulerWorkload != "preloaded" and schedulerWorkload != "external" and schedulerWorkload != "in-fiber" and
        schedulerWorkload != "balanced" and schedulerWorkload != "counter" and schedulerWorkload != "sustained")
    {
        return Result::Error("Unknown scheduler workload");
    }
    if (jobWorkers <= 0 or jobWorkers > 64)
    {
        return Result::Error("FiberJob worker count must be between one and 64");
    }
    if (jobRounds <= 0 or jobRounds > 15)
    {
        return Result::Error("FiberJob benchmark rounds must be between one and 15");
    }
    if (massSuspensionCommit != "full" and massSuspensionCommit != "incremental")
    {
        return Result::Error("Mass-suspension commitment must be full or incremental");
    }
    if (massSuspensionCount < 0 or (not schedulerThroughput and not jobThroughput and not jobPoolThroughput and
                                    not jobWorkerThroughput and not jobWorkerMatrix and not jobWorkerSustained and
                                    arguments.values.sizeInElements() != 0 and massSuspensionCount == 0))
    {
        return Result::Error("Mass-suspension fiber count must be greater than zero");
    }
    if (massSuspensionCount > 0)
    {
        return runMassSuspensionBenchmark(console, static_cast<size_t>(massSuspensionCount),
                                          massSuspensionCommit == "incremental" ? FiberStackCommitMode::Incremental
                                                                                : FiberStackCommitMode::Full);
    }
    if (jobThroughput)
    {
        return runFiberJobBenchmark(console);
    }
    if (jobPoolThroughput)
    {
        return runFiberJobPoolBenchmark(console);
    }
    if (jobWorkerThroughput)
    {
        return runFiberJobWorkerPoolBenchmark(console, static_cast<size_t>(jobWorkers));
    }
    if (jobWorkerMatrix)
    {
        return runFiberJobWorkerPoolBenchmarkMatrix(console, static_cast<size_t>(jobRounds));
    }
    if (jobWorkerSustained)
    {
        return runSustainedFiberJobBenchmark(console, static_cast<size_t>(jobWorkers));
    }
    if (schedulerThroughput)
    {
        for (int32_t round = 0; round < schedulerRounds; ++round)
        {
            if (schedulerWorkload == "all" or schedulerWorkload == "worker-pool")
            {
                SC_TRY(runWorkerPoolBenchmark(console));
            }
            if (schedulerWorkload == "all" or schedulerWorkload == "forced-steal")
            {
                SC_TRY(runForcedStealingBenchmark(console));
            }
            if (schedulerWorkload == "all" or schedulerWorkload == "preloaded" or schedulerWorkload == "external" or
                schedulerWorkload == "in-fiber" or schedulerWorkload == "balanced")
            {
                SC_TRY(runMicroTaskBenchmarks(console, static_cast<size_t>(externalProducers),
                                              static_cast<size_t>(schedulerWorkers), schedulerWorkload));
            }
            if (schedulerWorkload == "all" or schedulerWorkload == "counter")
            {
                SC_TRY(runCounterCompletionBenchmark(console));
            }
            if (schedulerWorkload == "all" or schedulerWorkload == "sustained")
            {
                SC_TRY(runSustainedMicroTaskBenchmark(console, static_cast<size_t>(schedulerWorkers)));
            }
        }
        return Result(true);
    }

    SC_TRY(runWorkerPoolBenchmark(console));
    SC_TRY(runForcedStealingBenchmark(console));
    SC_TRY(runMassSuspensionBenchmark(console, 10'000, FiberStackCommitMode::Full));
    SC_TRY(runMassSuspensionBenchmark(console, 100'000, FiberStackCommitMode::Full));
    SC_TRY(runAsyncFiberHighWaterBenchmark(console));
    SC_TRY(runMicroTaskBenchmarks(console, static_cast<size_t>(externalProducers), 0, "all"));
    SC_TRY(runCounterCompletionBenchmark(console));
    SC_TRY(runSustainedMicroTaskBenchmark(console, 0));
    return Result(true);
}
} // namespace SC

int main(int argc, const char* argv[])
{
    SC::Result result = SC::runFibersBenchmark(argc, argv);
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("FibersBenchmark failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
