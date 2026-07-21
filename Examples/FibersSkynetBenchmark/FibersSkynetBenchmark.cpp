// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Libraries/Fibers/Fibers.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
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

static Result runFibersSkynetBenchmark(int argc, const char* const* argv)
{
    int32_t    workers  = 4;
    int32_t    rounds   = 3;
    int32_t    maxDepth = 4;
    StringView backend  = "all";

    CommandLineOption options[4];
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
    options[2].help      = "Maximum fan-out depth (1-4 for the stackful backend)";
    options[2].value     = CommandLineValue::int32(maxDepth);

    options[3].longName  = "backend";
    options[3].valueName = "NAME";
    options[3].help      = "Backend to run: all, fibers, or taskflow";
    options[3].value     = CommandLineValue::stringView(backend);

    CommandLineSpec spec;
    spec.programName = "FibersSkynetBenchmark";
    spec.summary     = "Compare the SC stackful scheduler with Taskflow on the pinned Skynet workload.";
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

    SC_TRY_MSG(workers > 0 and rounds > 0 and maxDepth > 0 and maxDepth <= 4,
               "workers and rounds must be positive; max-depth must be between 1 and 4");
    SC_TRY_MSG(backend == "all" or backend == "fibers" or backend == "taskflow",
               "backend must be all, fibers, or taskflow");

    console.print("Skynet packageRevision=ec97c0095bd10907584a3b408e181410796b48fe workers={} rounds={}\n",
                  static_cast<size_t>(workers), static_cast<size_t>(rounds));
    console.print(
        "allocation: Fibers receives bounded task, 16 KiB virtual-stack, deque, and injection capacity before "
        "timing, then acquires slots and commits stack pages on demand; Taskflow uses its upstream runtime "
        "allocation policy\n");
    console.print("backend depth leaves expected reportedResult elapsedUs\n");

    for (int32_t depth = 1; depth <= maxDepth; ++depth)
    {
        const uint64_t leaves   = powerOfTen(static_cast<uint32_t>(depth));
        const uint64_t expected = leaves * (leaves - 1) / 2;

        if (backend == "all" or backend == "fibers")
        {
            int64_t  totalUs        = 0;
            uint64_t measuredResult = 0;
            for (int32_t round = 0; round < rounds; ++round)
            {
                int64_t elapsedUs = 0;
                SC_TRY(measureFibersSkynet(static_cast<uint32_t>(workers), static_cast<uint32_t>(depth), measuredResult,
                                           elapsedUs));
                SC_TRY_MSG(measuredResult == expected, "Fibers Skynet sum mismatch");
                totalUs += elapsedUs;
            }
            console.print("fibers {} {} {} {} {}\n", static_cast<size_t>(depth), static_cast<size_t>(leaves),
                          static_cast<size_t>(expected), static_cast<size_t>(measuredResult),
                          static_cast<size_t>(totalUs / rounds));
        }

        if (backend == "all" or backend == "taskflow")
        {
            int64_t totalUs = 0;
            for (int32_t round = 0; round < rounds; ++round)
            {
                totalUs += measure_time_taskflow(static_cast<size_t>(workers), static_cast<size_t>(depth)).count();
            }
            console.print("taskflow {} {} {} {} {}\n", static_cast<size_t>(depth), static_cast<size_t>(leaves),
                          static_cast<size_t>(expected), "upstream-not-reported",
                          static_cast<size_t>(totalUs / rounds));
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
