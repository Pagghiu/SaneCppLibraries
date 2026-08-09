// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// Renders a bounded grayscale Mandelbrot image with one stackless FiberJob per row.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run FibersMandelbrot -- mandelbrot.pgm --workers 4` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Common/Deferred.h"
#include "../../Libraries/Common/StringPath.h"
#include "../../Libraries/Fibers/Fibers.h"
#include "../../Libraries/File/File.h"
#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringBuilder.h"
#include "../../Libraries/Strings/StringView.h"

namespace SC
{
static constexpr size_t MandelbrotMaxWidth   = 1024;
static constexpr size_t MandelbrotMaxHeight  = 1024;
static constexpr size_t MandelbrotMaxWorkers = 16;

static Result resolveOutputPath(StringSpan argument, StringSpan currentDirectory, StringPath& path)
{
    if (Path::isAbsolute(StringView(argument), Path::AsNative))
    {
        SC_TRY_MSG(path.assign(argument), "FibersMandelbrot output path is too long");
        return Result(true);
    }

    StringView components[] = {StringView(currentDirectory), StringView(argument)};
    SC_TRY_MSG(Path::join(path, components), "FibersMandelbrot output path is too long");
    return Result(true);
}

struct FibersMandelbrotState
{
    FiberJob* jobs       = nullptr;
    uint8_t*  pixels     = nullptr;
    size_t    width      = 0;
    size_t    height     = 0;
    int32_t   iterations = 0;

    Result renderRow(FiberJobContext& context)
    {
        SC_TRY(context.checkCancellation());
        const size_t row = static_cast<size_t>(&context.job() - jobs);
        SC_TRY_MSG(row < height, "FibersMandelbrot job is outside the image");

        const double imaginary = -1.25 + 2.5 * static_cast<double>(row) / static_cast<double>(height - 1);
        for (size_t column = 0; column < width; ++column)
        {
            const double real = -2.5 + 3.5 * static_cast<double>(column) / static_cast<double>(width - 1);
            double       x    = 0.0;
            double       y    = 0.0;
            int32_t      step = 0;
            while (x * x + y * y <= 4.0 and step < iterations)
            {
                const double nextX = x * x - y * y + real;
                y                  = 2.0 * x * y + imaginary;
                x                  = nextX;
                step += 1;
            }
            pixels[row * width + column] = step == iterations ? 0 : static_cast<uint8_t>(255 - step * 255 / iterations);
        }
        return Result(true);
    }
};

static Result runFibersMandelbrot(int argc, const char* const* argv)
{
    StringSpan outputArgument;
    int32_t    width      = 800;
    int32_t    height     = 600;
    int32_t    workers    = 4;
    int32_t    iterations = 255;

    CommandLineOption options[4];
    options[0].longName  = "width";
    options[0].valueName = "PIXELS";
    options[0].help      = "Image width from 2 to 1024";
    options[0].value     = CommandLineValue::int32(width);
    options[1].longName  = "height";
    options[1].valueName = "PIXELS";
    options[1].help      = "Image height and bounded job count from 2 to 1024";
    options[1].value     = CommandLineValue::int32(height);
    options[2].longName  = "workers";
    options[2].valueName = "COUNT";
    options[2].help      = "Worker threads from 1 to 16";
    options[2].value     = CommandLineValue::int32(workers);
    options[3].longName  = "iterations";
    options[3].valueName = "COUNT";
    options[3].help      = "Maximum iterations from 1 to 255";
    options[3].value     = CommandLineValue::int32(iterations);

    CommandLinePositional positionals[1];
    positionals[0].name  = "output";
    positionals[0].help  = "Destination PGM image path";
    positionals[0].value = CommandLineValue::stringSpan(outputArgument);

    CommandLineSpec spec;
    spec.programName = "FibersMandelbrot";
    spec.summary     = "Render a bounded Mandelbrot image with stackless FiberJob workers.";
    spec.options     = options;
    spec.positionals = positionals;

    StringSpan           argumentStorage[16];
    CommandLineArguments arguments;
    SC_TRY(arguments.setFromMainArguments(argc, argv, argumentStorage));
    const CommandLineParseResult parseResult = spec.parse(arguments.values);

    Console console;
    Console::tryAttachingToParentConsole();
    if (parseResult.status == CommandLineParseResult::Status::HelpRequested)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, true);
        SC_TRY_MSG(spec.writeHelp(output), "Failed writing FibersMandelbrot help");
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, false);
        SC_TRY_MSG(spec.writeError(parseResult, output), "Failed writing FibersMandelbrot parse error");
        return Result::Error("Invalid FibersMandelbrot arguments");
    }

    SC_TRY_MSG(width >= 2 and width <= static_cast<int32_t>(MandelbrotMaxWidth), "width must be between 2 and 1024");
    SC_TRY_MSG(height >= 2 and height <= static_cast<int32_t>(MandelbrotMaxHeight),
               "height must be between 2 and 1024");
    SC_TRY_MSG(workers >= 1 and workers <= static_cast<int32_t>(MandelbrotMaxWorkers),
               "workers must be between 1 and 16");
    SC_TRY_MSG(iterations >= 1 and iterations <= 255, "iterations must be between 1 and 255");

    static FiberJob             jobs[MandelbrotMaxHeight];
    static FiberJob*            readyStorage[MandelbrotMaxHeight] = {};
    static FiberJobWorker       workerStorage[MandelbrotMaxWorkers];
    static FiberJobWorkerThread threadStorage[MandelbrotMaxWorkers];
    static uint8_t              pixels[MandelbrotMaxWidth * MandelbrotMaxHeight]                                   = {};
    static char                 dequeMemory[MandelbrotMaxWorkers * MandelbrotMaxHeight * sizeof(FiberJob*) + 4096] = {};

    FiberAllocator        allocator;
    FiberJobScheduler     scheduler;
    FiberJobWorkerPool    workerPool;
    FibersMandelbrotState state;
    state.jobs       = jobs;
    state.pixels     = pixels;
    state.width      = static_cast<size_t>(width);
    state.height     = static_cast<size_t>(height);
    state.iterations = iterations;

    SC_TRY(allocator.createFixed(dequeMemory));
    SC_TRY(scheduler.create({readyStorage, state.height}));

    FiberJobWorkerPoolOptions poolOptions;
    poolOptions.dequeAllocator         = &allocator;
    poolOptions.dequeCapacityPerWorker = state.height;
    poolOptions.keepAliveWhenIdle      = true;

    //! [FibersMandelbrotRun]
    SC_TRY(workerPool.start(scheduler, {workerStorage, static_cast<size_t>(workers)},
                            {threadStorage, static_cast<size_t>(workers)}, poolOptions));
    auto shutdownWorkers = MakeDeferred(
        [&workerPool]
        {
            if (workerPool.isRunning())
            {
                (void)workerPool.shutdown();
            }
        });

    FibersMandelbrotState* statePointer = &state;
    SC_TRY(scheduler.spawn({jobs, state.height}, FiberJob::Procedure([statePointer](FiberJobContext& context)
                                                                     { return statePointer->renderRow(context); })));
    SC_TRY(workerPool.waitIdle());
    SC_TRY(workerPool.requestStop());
    SC_TRY(workerPool.join());
    //! [FibersMandelbrotRun]

    for (size_t row = 0; row < state.height; ++row)
    {
        SC_TRY(jobs[row].result());
    }

    size_t executedJobs = 0;
    size_t stolenJobs   = 0;
    for (size_t workerIndex = 0; workerIndex < static_cast<size_t>(workers); ++workerIndex)
    {
        FiberJobWorkerDiagnostics diagnostics;
        scheduler.workerDiagnostics(workerStorage[workerIndex], diagnostics);
        executedJobs += diagnostics.executedJobs;
        stolenJobs += diagnostics.stolenJobs;
    }
    SC_TRY_MSG(executedJobs == state.height, "FibersMandelbrot executed an unexpected number of jobs");

    StringPath       currentDirectoryStorage;
    const StringSpan currentDirectory = FileSystem::Operations::getCurrentWorkingDirectory(currentDirectoryStorage);
    SC_TRY_MSG(not currentDirectory.isEmpty(), "FibersMandelbrot could not resolve the current directory");
    StringPath outputPath;
    SC_TRY(resolveOutputPath(outputArgument, currentDirectory, outputPath));

    FileDescriptor outputFile;
    SC_TRY(outputFile.open(outputPath.view(), FileOpen::Write));
    auto closeOutput = MakeDeferred([&outputFile] { (void)outputFile.close(); });

    SmallString<64> header;
    SC_TRY(StringBuilder::format(header, "P5\n{} {}\n255\n", state.width, state.height));
    SC_TRY(outputFile.writeString(header.view()));
    SC_TRY(outputFile.write({pixels, state.width * state.height}));

    SC_TRY(scheduler.close());
    SC_TRY(allocator.close());
    console.print("Rendered {}x{} Mandelbrot image with {} workers, {} jobs, and {} stolen jobs to {}\n", state.width,
                  state.height, static_cast<size_t>(workers), executedJobs, stolenJobs, outputPath.view());
    return Result(true);
}
} // namespace SC

int main(int argc, const char* const* argv)
{
    const SC::Result result = SC::runFibersMandelbrot(argc, argv);
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("FibersMandelbrot failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
