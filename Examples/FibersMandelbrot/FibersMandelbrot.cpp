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

#include "../../Libraries/Fibers/FibersErrorFormatter.h"
#include "../../Libraries/File/FileErrorFormatter.h"
#include "../../Libraries/FileSystem/FileSystemErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory FibersMandelbrotResultCategory = ResultCategory(0x80000008u);
enum class MandelbrotExampleError : uint32_t
{
    InvalidArguments = 1,
    OutputPathTooLong,
    RowOutsideImage,
    HelpWriteFailed,
    ParseErrorWriteFailed,
    WidthOutOfRange,
    HeightOutOfRange,
    WorkerCountOutOfRange,
    IterationCountOutOfRange,
    JobCountMismatch,
    CurrentDirectoryUnavailable,
};
static constexpr Result FibersMandelbrotFailure(MandelbrotExampleError error)
{
    return Result::Error(FibersMandelbrotResultCategory, error);
}
static constexpr Result FibersMandelbrotCheck(bool condition, MandelbrotExampleError error)
{
    return condition ? Result(true) : FibersMandelbrotFailure(error);
}
static ResultErrorFormat formatFibersMandelbrotError(Result result, Span<char> output)
{
    if (result.category() == FibersResultCategory)
        return formatFibersError(result, output);
    if (result.category() == FileResultCategory)
        return formatFileError(result, output);
    if (result.category() == FileSystemResultCategory)
        return formatFileSystemError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() != FibersMandelbrotResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<MandelbrotExampleError>(result.errorValue()))
    {
    case MandelbrotExampleError::InvalidArguments: formatter.append("Invalid FibersMandelbrot arguments"); break;
    case MandelbrotExampleError::OutputPathTooLong: formatter.append("Output path is too long"); break;
    case MandelbrotExampleError::RowOutsideImage: formatter.append("Job is outside the image"); break;
    case MandelbrotExampleError::HelpWriteFailed: formatter.append("Failed writing FibersMandelbrot help"); break;
    case MandelbrotExampleError::ParseErrorWriteFailed:
        formatter.append("Failed writing FibersMandelbrot parse error");
        break;
    case MandelbrotExampleError::WidthOutOfRange: formatter.append("Width must be between 2 and 1024"); break;
    case MandelbrotExampleError::HeightOutOfRange: formatter.append("Height must be between 2 and 1024"); break;
    case MandelbrotExampleError::WorkerCountOutOfRange: formatter.append("Workers must be between 1 and 16"); break;
    case MandelbrotExampleError::IterationCountOutOfRange:
        formatter.append("Iterations must be between 1 and 255");
        break;
    case MandelbrotExampleError::JobCountMismatch: formatter.append("Executed an unexpected number of jobs"); break;
    case MandelbrotExampleError::CurrentDirectoryUnavailable:
        formatter.append("Could not resolve the current directory");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static constexpr size_t MandelbrotMaxWidth   = 1024;
static constexpr size_t MandelbrotMaxHeight  = 1024;
static constexpr size_t MandelbrotMaxWorkers = 16;

static Result resolveOutputPath(StringSpan argument, StringSpan currentDirectory, StringPath& path)
{
    if (Path::isAbsolute(StringView(argument), Path::AsNative))
    {
        SC_TRY(FibersMandelbrotCheck(path.assign(argument), MandelbrotExampleError::OutputPathTooLong));
        return Result(true);
    }

    StringView components[] = {StringView(currentDirectory), StringView(argument)};
    SC_TRY(FibersMandelbrotCheck(Path::join(path, components), MandelbrotExampleError::OutputPathTooLong));
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
        SC_TRY(FibersMandelbrotCheck(row < height, MandelbrotExampleError::RowOutsideImage));

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
        SC_TRY(FibersMandelbrotCheck(spec.writeHelp(output), MandelbrotExampleError::HelpWriteFailed));
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, false);
        SC_TRY(
            FibersMandelbrotCheck(spec.writeError(parseResult, output), MandelbrotExampleError::ParseErrorWriteFailed));
        return FibersMandelbrotFailure(MandelbrotExampleError::InvalidArguments);
    }

    SC_TRY(FibersMandelbrotCheck(width >= 2 and width <= static_cast<int32_t>(MandelbrotMaxWidth),
                                 MandelbrotExampleError::WidthOutOfRange));
    SC_TRY(FibersMandelbrotCheck(height >= 2 and height <= static_cast<int32_t>(MandelbrotMaxHeight),
                                 MandelbrotExampleError::HeightOutOfRange));
    SC_TRY(FibersMandelbrotCheck(workers >= 1 and workers <= static_cast<int32_t>(MandelbrotMaxWorkers),
                                 MandelbrotExampleError::WorkerCountOutOfRange));
    SC_TRY(
        FibersMandelbrotCheck(iterations >= 1 and iterations <= 255, MandelbrotExampleError::IterationCountOutOfRange));

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
    SC_TRY(FibersMandelbrotCheck(executedJobs == state.height, MandelbrotExampleError::JobCountMismatch));

    StringPath       currentDirectoryStorage;
    const StringSpan currentDirectory = FileSystem::Operations::getCurrentWorkingDirectory(currentDirectoryStorage);
    SC_TRY(FibersMandelbrotCheck(not currentDirectory.isEmpty(), MandelbrotExampleError::CurrentDirectoryUnavailable));
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
        char                        message[256];
        const SC::ResultErrorFormat formatted = SC::formatFibersMandelbrotError(result, message);
        if (formatted)
            console.print("FibersMandelbrot failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("FibersMandelbrot failed: error category {}, code {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
