// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Libraries/Fibers/Fibers.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringBuilder.h"
#include "../../Libraries/Strings/StringFormat.h"
#include "../../Libraries/Threading/Atomic.h"
#include "../../Libraries/Time/Time.h"

#include "skynet.hpp"

#include <new>

namespace SC
{
struct SkynetNode
{
    uint64_t baseNumber = 0;
    uint64_t result     = 0;
    uint32_t depth      = 0;
    uint32_t firstChild = 0;
};

struct FibersSkynetState
{
    FiberScheduler* scheduler = nullptr;
    FiberTaskPool*  taskPool  = nullptr;
    SkynetNode*     nodes     = nullptr;
    uint32_t        maxDepth  = 0;
    Atomic<int32_t> nextNode;
    Atomic<int32_t> failedTasks;
};

struct FiberJobSkynetNode
{
    FiberJobSkynetNode* parent     = nullptr;
    uint64_t            baseNumber = 0;
    uint64_t            result     = 0;
    uint32_t            depth      = 0;
    uint32_t            firstChild = 0;
    Atomic<int32_t>     pendingChildren;
};

struct FiberJobSkynetState
{
    FiberJobScheduler*  scheduler     = nullptr;
    FiberJobSkynetNode* nodes         = nullptr;
    FiberJob*           jobs          = nullptr;
    FiberJob*           continuations = nullptr;
    uint32_t            maxDepth      = 0;
    Atomic<int32_t>     nextNode;
    Atomic<int32_t>     failedJobs;
};

static uint64_t powerOfTen(uint32_t exponent)
{
    uint64_t value = 1;
    while (exponent > 0)
    {
        value *= 10;
        exponent -= 1;
    }
    return value;
}

static uint32_t nodeCountForDepth(uint32_t depth)
{
    uint32_t count = 1;
    uint32_t level = 1;
    for (uint32_t current = 0; current < depth; ++current)
    {
        level *= 10;
        count += level;
    }
    return count;
}

static void sortElapsedSamples(Span<int64_t> samples)
{
    for (size_t index = 1; index < samples.sizeInElements(); ++index)
    {
        const int64_t sample = samples[index];
        size_t        insert = index;
        while (insert > 0 and samples[insert - 1] > sample)
        {
            samples[insert] = samples[insert - 1];
            --insert;
        }
        samples[insert] = sample;
    }
}

static void printSkynetSamples(Console& console, StringView backend, uint32_t depth, uint64_t leaves, uint64_t expected,
                               StringView reportedResult, Span<int64_t> samples)
{
    int64_t total = 0;
    for (int64_t sample : samples)
    {
        total += sample;
    }
    sortElapsedSamples(samples);
    int64_t median = samples[samples.sizeInElements() / 2];
    if (samples.sizeInElements() % 2 == 0)
    {
        const int64_t lower = samples[samples.sizeInElements() / 2 - 1];
        median              = lower + (median - lower) / 2;
    }
    console.print("{} {} {} {} {} {} {} {} {}\n", backend, static_cast<size_t>(depth), static_cast<size_t>(leaves),
                  static_cast<size_t>(expected), reportedResult, static_cast<size_t>(samples[0]),
                  static_cast<size_t>(median), static_cast<size_t>(total / samples.sizeInElements()),
                  static_cast<size_t>(samples[samples.sizeInElements() - 1]));
}

static Result runFibersSkynetNode(FibersSkynetState& state, SkynetNode& node)
{
    if (node.depth == state.maxDepth)
    {
        node.result = node.baseNumber;
        return Result(true);
    }

    FiberCounter  counter;
    const int32_t firstChild = state.nextNode.fetch_add(10, memory_order_relaxed);
    SC_TRY_MSG(firstChild >= 0, "Skynet node index overflowed");
    node.firstChild = static_cast<uint32_t>(firstChild);

    const uint64_t depthOffset = powerOfTen(state.maxDepth - node.depth - 1);
    for (uint32_t childIndex = 0; childIndex < 10; ++childIndex)
    {
        SkynetNode& child = state.nodes[node.firstChild + childIndex];
        child.baseNumber  = node.baseNumber + depthOffset * childIndex;
        child.depth       = node.depth + 1;
        SC_TRY(state.taskPool->spawn(*state.scheduler,
                                     FiberTask::Procedure(
                                         [&state, childPointer = &child](FiberScheduler&)
                                         {
                                             Result result = runFibersSkynetNode(state, *childPointer);
                                             if (not result)
                                             {
                                                 state.failedTasks.fetch_add(1, memory_order_relaxed);
                                             }
                                             return result;
                                         }),
                                     nullptr, &counter));
    }

    SC_TRY(state.scheduler->wait(counter));
    for (uint32_t childIndex = 0; childIndex < 10; ++childIndex)
    {
        node.result += state.nodes[node.firstChild + childIndex].result;
    }
    return Result(true);
}

static Result runFiberJobSkynetNode(FiberJobSkynetState& state, FiberJobSkynetNode& node);
static Result completeFiberJobSkynetNode(FiberJobSkynetState& state, FiberJobSkynetNode& node);

static void recordFiberJobSkynetResult(FiberJobSkynetState& state, Result result)
{
    if (not result)
    {
        state.failedJobs.fetch_add(1, memory_order_release);
    }
}

static Result spawnFiberJobSkynetNode(FiberJobSkynetState& state, FiberJobSkynetNode& node)
{
    FiberJobSkynetState* statePointer = &state;
    const size_t         nodeIndex    = static_cast<size_t>(&node - state.nodes);
    return state.scheduler->spawn(
        state.jobs[nodeIndex], FiberJob::Procedure(
                                   [statePointer](FiberJobContext& context)
                                   {
                                       const size_t index = static_cast<size_t>(&context.job() - statePointer->jobs);
                                       Result result = runFiberJobSkynetNode(*statePointer, statePointer->nodes[index]);
                                       recordFiberJobSkynetResult(*statePointer, result);
                                       return result;
                                   }));
}

static Result spawnFiberJobSkynetContinuation(FiberJobSkynetState& state, FiberJobSkynetNode& node)
{
    FiberJobSkynetState* statePointer = &state;
    const size_t         nodeIndex    = static_cast<size_t>(&node - state.nodes);
    return state.scheduler->spawn(state.continuations[nodeIndex],
                                  FiberJob::Procedure(
                                      [statePointer](FiberJobContext& context)
                                      {
                                          const size_t index =
                                              static_cast<size_t>(&context.job() - statePointer->continuations);
                                          FiberJobSkynetNode* nodePointer = &statePointer->nodes[index];
                                          Result              result(true);
                                          for (uint32_t childIndex = 0; childIndex < 10; ++childIndex)
                                          {
                                              nodePointer->result +=
                                                  statePointer->nodes[nodePointer->firstChild + childIndex].result;
                                          }
                                          if (nodePointer->parent != nullptr)
                                          {
                                              result = completeFiberJobSkynetNode(*statePointer, *nodePointer);
                                          }
                                          recordFiberJobSkynetResult(*statePointer, result);
                                          return result;
                                      }));
}

static Result completeFiberJobSkynetNode(FiberJobSkynetState& state, FiberJobSkynetNode& node)
{
    if (node.parent == nullptr)
    {
        return Result(true);
    }
    const int32_t previousPending = node.parent->pendingChildren.fetch_sub(1, memory_order_acq_rel);
    SC_TRY_MSG(previousPending > 0, "FiberJob Skynet parent completion underflow");
    if (previousPending == 1)
    {
        SC_TRY(spawnFiberJobSkynetContinuation(state, *node.parent));
    }
    return Result(true);
}

static Result runFiberJobSkynetNode(FiberJobSkynetState& state, FiberJobSkynetNode& node)
{
    if (node.depth == state.maxDepth)
    {
        node.result = node.baseNumber;
        return completeFiberJobSkynetNode(state, node);
    }
    const int32_t firstChild = state.nextNode.fetch_add(10, memory_order_relaxed);
    SC_TRY_MSG(firstChild >= 0, "FiberJob Skynet node index overflowed");
    node.firstChild = static_cast<uint32_t>(firstChild);
    node.pendingChildren.store(10, memory_order_relaxed);

    const uint64_t depthOffset = powerOfTen(state.maxDepth - node.depth - 1);
    for (uint32_t childIndex = 0; childIndex < 10; ++childIndex)
    {
        FiberJobSkynetNode& child = state.nodes[node.firstChild + childIndex];
        child.parent              = &node;
        child.baseNumber          = node.baseNumber + depthOffset * childIndex;
        child.depth               = node.depth + 1;
    }
    FiberJobSkynetState* statePointer = &state;
    return state.scheduler->spawn({state.jobs + node.firstChild, 10},
                                  FiberJob::Procedure(
                                      [statePointer](FiberJobContext& context)
                                      {
                                          const size_t index = static_cast<size_t>(&context.job() - statePointer->jobs);
                                          Result       result =
                                              runFiberJobSkynetNode(*statePointer, statePointer->nodes[index]);
                                          recordFiberJobSkynetResult(*statePointer, result);
                                          return result;
                                      }));
}

static Result measureFibersSkynet(uint32_t numWorkers, uint32_t maxDepth, uint64_t& result, int64_t& elapsedUs)
{
    static constexpr size_t StackSize              = 16 * 1024;
    static constexpr size_t DequeCapacityPerWorker = 1024;

    const uint32_t numNodes = nodeCountForDepth(maxDepth);

    SkynetNode*        nodes   = new (std::nothrow) SkynetNode[numNodes];
    FiberWorker*       workers = new (std::nothrow) FiberWorker[numWorkers];
    FiberWorkerThread* threads = new (std::nothrow) FiberWorkerThread[numWorkers];

    if (nodes == nullptr or workers == nullptr or threads == nullptr)
    {
        delete[] threads;
        delete[] workers;
        delete[] nodes;
        return Result::Error("Cannot allocate caller-owned Skynet benchmark storage");
    }

    FiberScheduler  scheduler;
    FiberWorkerPool workerPool;
    FiberAllocator  allocator;
    FiberTaskClass  taskClass;
    FiberStackClass stackClass;
    FiberTaskPool   taskPool;

    FibersSkynetState state;
    state.scheduler = &scheduler;
    state.taskPool  = &taskPool;
    state.nodes     = nodes;
    state.maxDepth  = maxDepth;
    state.nextNode.store(1, memory_order_relaxed);
    state.failedTasks.store(0, memory_order_relaxed);

    FiberWorkerPoolOptions options;
    options.dequeAllocator         = &allocator;
    options.dequeCapacityPerWorker = DequeCapacityPerWorker;
    options.injectionAllocator     = &allocator;
    options.injectionCapacity      = numNodes + 1;

    FiberAllocatorVirtualOptions allocatorOptions;
    allocatorOptions.reserveBytes = static_cast<size_t>(numNodes) * sizeof(FiberTask) * 2 +
                                    static_cast<size_t>(numWorkers) * DequeCapacityPerWorker * sizeof(FiberTask*) +
                                    static_cast<size_t>(numNodes + 1) * FiberInjectionSlotStorageSize + 4 * 1024 * 1024;
    allocatorOptions.initialCommitBytes = 64 * 1024;

    Result benchmarkResult = allocator.createVirtual(allocatorOptions);
    if (benchmarkResult)
    {
        FiberTaskClassOptions taskOptions;
        taskOptions.maxTasks = numNodes;
        benchmarkResult      = taskClass.create(allocator, taskOptions);
    }
    if (benchmarkResult)
    {
        FiberStackClassOptions stackOptions;
        stackOptions.stackSizeInBytes = StackSize;
        stackOptions.maxStacks        = numNodes;
        stackOptions.guardPage        = true;
        benchmarkResult               = stackClass.reserve(stackOptions);
    }
    if (benchmarkResult)
    {
        benchmarkResult = taskPool.create(taskClass, stackClass);
    }
    if (benchmarkResult)
    {
        benchmarkResult =
            taskPool.spawn(scheduler, FiberTask::Procedure([&state](FiberScheduler&)
                                                           { return runFibersSkynetNode(state, state.nodes[0]); }));
    }

    Time::HighResolutionCounter start;
    Time::HighResolutionCounter finish;
    if (benchmarkResult)
    {
        start.snap();
        benchmarkResult = workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, options);
    }
    if (benchmarkResult)
    {
        benchmarkResult = workerPool.join();
        finish.snap();
    }
    if (benchmarkResult)
    {
        result    = nodes[0].result;
        elapsedUs = finish.subtractExact(start).toNanoseconds().ns / 1000;
        if (state.failedTasks.load(memory_order_relaxed) != 0)
        {
            benchmarkResult = Result::Error("A Fibers Skynet child task failed");
        }
    }

    Result closeResult = taskPool.close();
    if (closeResult)
    {
        closeResult = taskClass.close();
    }
    stackClass.release();
    if (closeResult)
    {
        closeResult = allocator.close();
    }
    if (benchmarkResult and not closeResult)
    {
        benchmarkResult = closeResult;
    }

    delete[] threads;
    delete[] workers;
    delete[] nodes;
    return benchmarkResult;
}

struct FiberJobsSkynetRuntime
{
    static constexpr size_t DequeCapacityPerWorker = 1024;

    FiberJobSkynetNode*   nodes         = nullptr;
    FiberJob*             jobs          = nullptr;
    FiberJob*             continuations = nullptr;
    FiberJob**            readyStorage  = nullptr;
    FiberJobWorker*       workers       = nullptr;
    FiberJobWorkerThread* threads       = nullptr;

    uint32_t numNodes   = 0;
    uint32_t numWorkers = 0;

    FiberJobScheduler   scheduler;
    FiberJobWorkerPool  workerPool;
    FiberAllocator      allocator;
    FiberJobSkynetState state;

    Result create(uint32_t workersCount, uint32_t depth, size_t idleSpinAttempts)
    {
        numNodes   = nodeCountForDepth(depth);
        numWorkers = workersCount;

        nodes         = new (std::nothrow) FiberJobSkynetNode[numNodes];
        jobs          = new (std::nothrow) FiberJob[numNodes];
        continuations = new (std::nothrow) FiberJob[numNodes];
        readyStorage  = new (std::nothrow) FiberJob*[numNodes];
        workers       = new (std::nothrow) FiberJobWorker[numWorkers];
        threads       = new (std::nothrow) FiberJobWorkerThread[numWorkers];

        if (nodes == nullptr or jobs == nullptr or continuations == nullptr or readyStorage == nullptr or
            workers == nullptr or threads == nullptr)
        {
            static_cast<void>(close());
            return Result::Error("Cannot allocate caller-owned FiberJob Skynet benchmark storage");
        }

        state.scheduler     = &scheduler;
        state.nodes         = nodes;
        state.jobs          = jobs;
        state.continuations = continuations;
        state.maxDepth      = depth;

        FiberJobWorkerPoolOptions options;
        options.dequeAllocator         = &allocator;
        options.dequeCapacityPerWorker = DequeCapacityPerWorker;
        options.idleSpinAttempts       = idleSpinAttempts;
        options.keepAliveWhenIdle      = true;

        FiberAllocatorVirtualOptions allocatorOptions;
        allocatorOptions.reserveBytes =
            static_cast<size_t>(numWorkers) * DequeCapacityPerWorker * sizeof(FiberJob*) + 1024 * 1024;
        allocatorOptions.initialCommitBytes = 64 * 1024;

        Result result = allocator.createVirtual(allocatorOptions);
        if (result)
        {
            result = scheduler.create({readyStorage, numNodes});
        }
        if (result)
        {
            result = workerPool.start(scheduler, {workers, numWorkers}, {threads, numWorkers}, options);
        }
        if (not result)
        {
            static_cast<void>(close());
        }
        return result;
    }

    Result measure(uint64_t& result, int64_t& elapsedUs)
    {
        state.nextNode.store(1, memory_order_relaxed);
        state.failedJobs.store(0, memory_order_relaxed);
        for (uint32_t index = 0; index < numNodes; ++index)
        {
            FiberJobSkynetNode& node = nodes[index];
            node.parent              = nullptr;
            node.baseNumber          = 0;
            node.result              = 0;
            node.depth               = 0;
            node.firstChild          = 0;
            node.pendingChildren.store(0, memory_order_relaxed);
        }

        Time::HighResolutionCounter start;
        Time::HighResolutionCounter finish;
        start.snap();
        SC_TRY(spawnFiberJobSkynetNode(state, nodes[0]));
        SC_TRY(workerPool.waitIdle());
        finish.snap();

        result    = nodes[0].result;
        elapsedUs = finish.subtractExact(start).toNanoseconds().ns / 1000;
        SC_TRY_MSG(state.failedJobs.load(memory_order_acquire) == 0, "A FiberJob Skynet node failed");
        return Result(true);
    }

    Result close()
    {
        Result firstError = Result(true);
        if (workerPool.isRunning())
        {
            Result stopResult = workerPool.requestStop();
            if (firstError and not stopResult)
            {
                firstError = stopResult;
            }
            Result joinResult = workerPool.join();
            if (firstError and not joinResult)
            {
                firstError = joinResult;
            }
        }
        if (scheduler.isOpen())
        {
            Result schedulerResult = scheduler.close();
            if (firstError and not schedulerResult)
            {
                firstError = schedulerResult;
            }
        }
        Result allocatorResult = allocator.close();
        if (firstError and not allocatorResult)
        {
            firstError = allocatorResult;
        }

        delete[] threads;
        delete[] workers;
        delete[] readyStorage;
        delete[] continuations;
        delete[] jobs;
        delete[] nodes;
        threads       = nullptr;
        workers       = nullptr;
        readyStorage  = nullptr;
        continuations = nullptr;
        jobs          = nullptr;
        nodes         = nullptr;
        return firstError;
    }
};

struct FiberJobsSkynetSampleOptions
{
    uint32_t numWorkers       = 0;
    uint32_t depth            = 0;
    int32_t  rounds           = 0;
    uint64_t leaves           = 0;
    uint64_t expected         = 0;
    size_t   idleSpinAttempts = 0;
};

static Result measureFiberJobsSkynetSamples(Console& console, const FiberJobsSkynetSampleOptions& options)
{
    FiberJobsSkynetRuntime runtime;
    SC_TRY(runtime.create(options.numWorkers, options.depth, options.idleSpinAttempts));

    uint64_t measuredResult = 0;
    int64_t  warmupUs       = 0;
    Result   result         = runtime.measure(measuredResult, warmupUs);
    if (result and measuredResult != options.expected)
    {
        result = Result::Error("FiberJob Skynet warm-up sum mismatch");
    }

    int64_t elapsedSamples[15] = {};
    for (int32_t round = 0; result and round < options.rounds; ++round)
    {
        result = runtime.measure(measuredResult, elapsedSamples[round]);
        if (result and measuredResult != options.expected)
        {
            result = Result::Error("FiberJob Skynet sum mismatch");
        }
    }

    Result closeResult = runtime.close();
    if (result and not closeResult)
    {
        result = closeResult;
    }
    SC_TRY(result);

    SmallString<32> resultText(StringEncoding::Ascii);
    SC_TRY(StringBuilder::format(resultText, "{}", measuredResult));
    printSkynetSamples(console, "jobs", options.depth, options.leaves, options.expected, resultText.view(),
                       {elapsedSamples, static_cast<size_t>(options.rounds)});
    return Result(true);
}

static Result runFibersSkynetBenchmark(int argc, const char* const* argv)
{
    int32_t    workers             = 4;
    int32_t    rounds              = 3;
    int32_t    maxDepth            = 4;
    int32_t    jobIdleSpinAttempts = 32;
    StringView backend             = "all";

    CommandLineOption options[5];
    options[0].longName  = "workers";
    options[0].valueName = "COUNT";
    options[0].help      = "Worker threads used by both backends";
    options[0].value     = CommandLineValue::int32(workers);

    options[1].longName  = "rounds";
    options[1].valueName = "COUNT";
    options[1].help      = "Measured repetitions per depth and backend";
    options[1].value     = CommandLineValue::int32(rounds);

    options[2].longName  = "max-depth";
    options[2].valueName = "DEPTH";
    options[2].help      = "Maximum fan-out depth (1-4 for all/fibers, 1-6 for jobs/taskflow)";
    options[2].value     = CommandLineValue::int32(maxDepth);

    options[3].longName  = "backend";
    options[3].valueName = "NAME";
    options[3].help      = "Backend to run: all, fibers, jobs, or taskflow";
    options[3].value     = CommandLineValue::stringView(backend);

    options[4].longName  = "job-idle-spins";
    options[4].valueName = "COUNT";
    options[4].help      = "FiberJob CPU-relax attempts before parking an idle worker";
    options[4].value     = CommandLineValue::int32(jobIdleSpinAttempts);

    CommandLineSpec spec;
    spec.programName = "FibersSkynetBenchmark";
    spec.summary     = "Compare SC stackful tasks, stackless jobs, and Taskflow on the pinned Skynet workload.";
    spec.options     = options;

    StringSpan           argumentStorage[16];
    CommandLineArguments arguments;
    SC_TRY(arguments.setFromMainArguments(argc, argv, argumentStorage));
    const CommandLineParseResult parseResult = spec.parse(arguments.values);

    Console console;
    Console::tryAttachingToParentConsole();
    if (parseResult.status == CommandLineParseResult::Status::HelpRequested)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, true);
        SC_TRY_MSG(spec.writeHelp(output), "Failed writing FibersSkynetBenchmark help");
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, false);
        SC_TRY_MSG(spec.writeError(parseResult, output), "Failed writing FibersSkynetBenchmark parse error");
        return Result::Error("Invalid FibersSkynetBenchmark arguments");
    }

    SC_TRY_MSG(workers > 0 and rounds > 0 and rounds <= 15 and maxDepth > 0 and maxDepth <= 6,
               "workers must be positive; rounds and max-depth must be between 1 and 15 and 1 and 6 respectively");
    SC_TRY_MSG(jobIdleSpinAttempts >= 0, "job-idle-spins must not be negative");
    SC_TRY_MSG(backend == "all" or backend == "fibers" or backend == "jobs" or backend == "taskflow",
               "backend must be all, fibers, jobs, or taskflow");
    SC_TRY_MSG((backend != "all" and backend != "fibers") or maxDepth <= 4,
               "all and fibers backends require max-depth between 1 and 4");

    console.print(
        "Skynet packageRevision=ec97c0095bd10907584a3b408e181410796b48fe workers={} rounds={} jobIdleSpins={}\n",
        static_cast<size_t>(workers), static_cast<size_t>(rounds), static_cast<size_t>(jobIdleSpinAttempts));
    console.print(
        "allocation: Fibers receives bounded task, 16 KiB virtual-stack, deque, and injection capacity; FiberJob "
        "receives stable node, initial/continuation-job, ready-pointer, worker, thread, and deque capacity; all SC "
        "capacity is reserved "
        "before timing and runtime slots come only from those explicit bounds, while Taskflow uses its upstream "
        "runtime allocation policy\n");
    console.print(
        "timing: one warm-up precedes every backend/depth sample set; FiberTask samples include worker-pool startup "
        "and shutdown, while FiberJob samples reuse one persistent pool per depth and the unchanged upstream Taskflow "
        "function reuses its static executor\n");
    console.print(
        "backend depth leaves expected reportedResult elapsedUsMin elapsedUsMedian elapsedUsMean elapsedUsMax\n");

    for (int32_t depth = 1; depth <= maxDepth; ++depth)
    {
        const uint64_t leaves   = powerOfTen(static_cast<uint32_t>(depth));
        const uint64_t expected = leaves * (leaves - 1) / 2;

        if (backend == "all" or backend == "fibers")
        {
            uint64_t measuredResult = 0;
            int64_t  warmupUs       = 0;
            SC_TRY(measureFibersSkynet(static_cast<uint32_t>(workers), static_cast<uint32_t>(depth), measuredResult,
                                       warmupUs));
            SC_TRY_MSG(measuredResult == expected, "Fibers Skynet warm-up sum mismatch");

            int64_t elapsedSamples[15] = {};
            for (int32_t round = 0; round < rounds; ++round)
            {
                SC_TRY(measureFibersSkynet(static_cast<uint32_t>(workers), static_cast<uint32_t>(depth), measuredResult,
                                           elapsedSamples[round]));
                SC_TRY_MSG(measuredResult == expected, "Fibers Skynet sum mismatch");
            }
            SmallString<32> resultText(StringEncoding::Ascii);
            SC_TRY(StringBuilder::format(resultText, "{}", measuredResult));
            printSkynetSamples(console, "fibers", static_cast<uint32_t>(depth), leaves, expected, resultText.view(),
                               {elapsedSamples, static_cast<size_t>(rounds)});
        }

        if (backend == "all" or backend == "jobs")
        {
            FiberJobsSkynetSampleOptions sampleOptions;
            sampleOptions.numWorkers       = static_cast<uint32_t>(workers);
            sampleOptions.depth            = static_cast<uint32_t>(depth);
            sampleOptions.rounds           = rounds;
            sampleOptions.leaves           = leaves;
            sampleOptions.expected         = expected;
            sampleOptions.idleSpinAttempts = static_cast<size_t>(jobIdleSpinAttempts);
            SC_TRY(measureFiberJobsSkynetSamples(console, sampleOptions));
        }

        if (backend == "all" or backend == "taskflow")
        {
            static_cast<void>(measure_time_taskflow(static_cast<size_t>(workers), static_cast<size_t>(depth)));
            int64_t elapsedSamples[15] = {};
            for (int32_t round = 0; round < rounds; ++round)
            {
                elapsedSamples[round] =
                    measure_time_taskflow(static_cast<size_t>(workers), static_cast<size_t>(depth)).count();
            }
            printSkynetSamples(console, "taskflow", static_cast<uint32_t>(depth), leaves, expected,
                               "upstream-not-reported", {elapsedSamples, static_cast<size_t>(rounds)});
        }
    }
    return Result(true);
}
} // namespace SC

int main(int argc, const char* argv[])
{
    SC::Result result = SC::runFibersSkynetBenchmark(argc, argv);
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("FibersSkynetBenchmark failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
