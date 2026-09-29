// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// Copies a file through a bounded AsyncFibers read/write loop.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AsyncFibersFileCopy -- <source> <destination>` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/AsyncFibers/AsyncFibers.h"
#include "../../Libraries/Common/Deferred.h"
#include "../../Libraries/Common/StringPath.h"
#include "../../Libraries/File/File.h"
#include "../../Libraries/FileSystem/FileSystem.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringView.h"

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/AsyncFibers/AsyncFibersErrorFormatter.h"
#include "../../Libraries/Fibers/FibersErrorFormatter.h"
#include "../../Libraries/File/FileErrorFormatter.h"
#include "../../Libraries/FileSystem/FileSystemErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AsyncFibersFileCopyResultCategory = ResultCategory(0x80000009u);
enum class FileCopyExampleError : uint32_t
{
    ReadNoProgress = 1,
    InvalidArguments,
    PathTooLong,
    ShortWrite,
    HelpWriteFailed,
    ParseErrorWriteFailed,
    CurrentDirectoryUnavailable,
    SamePath,
    SourceNotFile,
    DestinationNotFile,
    SameFile,
};
static constexpr Result AsyncFibersFileCopyFailure(FileCopyExampleError error)
{
    return Result::Error(AsyncFibersFileCopyResultCategory, error);
}
static constexpr Result AsyncFibersFileCopyCheck(bool condition, FileCopyExampleError error)
{
    return condition ? Result(true) : AsyncFibersFileCopyFailure(error);
}
static ResultErrorFormat formatAsyncFibersFileCopyError(Result result, Span<char> output)
{
    if (result.category() == AsyncFibersResultCategory)
        return formatAsyncFibersError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == FibersResultCategory)
        return formatFibersError(result, output);
    if (result.category() == FileResultCategory)
        return formatFileError(result, output);
    if (result.category() == FileSystemResultCategory)
        return formatFileSystemError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() != AsyncFibersFileCopyResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<FileCopyExampleError>(result.errorValue()))
    {
    case FileCopyExampleError::ReadNoProgress: formatter.append("Read made no progress"); break;
    case FileCopyExampleError::InvalidArguments: formatter.append("Invalid AsyncFibersFileCopy arguments"); break;
    case FileCopyExampleError::PathTooLong: formatter.append("Path is too long"); break;
    case FileCopyExampleError::ShortWrite: formatter.append("Completed a short write"); break;
    case FileCopyExampleError::HelpWriteFailed: formatter.append("Failed writing AsyncFibersFileCopy help"); break;
    case FileCopyExampleError::ParseErrorWriteFailed:
        formatter.append("Failed writing AsyncFibersFileCopy parse error");
        break;
    case FileCopyExampleError::CurrentDirectoryUnavailable:
        formatter.append("Could not resolve the current directory");
        break;
    case FileCopyExampleError::SamePath: formatter.append("Source and destination must differ"); break;
    case FileCopyExampleError::SourceNotFile: formatter.append("Source is not a file"); break;
    case FileCopyExampleError::DestinationNotFile: formatter.append("Destination is not a file"); break;
    case FileCopyExampleError::SameFile: formatter.append("Source and destination identify the same file"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static Result resolvePath(StringSpan argument, StringSpan currentDirectory, StringPath& path)
{
    if (Path::isAbsolute(StringView(argument), Path::AsNative))
    {
        SC_TRY(AsyncFibersFileCopyCheck(path.assign(argument), FileCopyExampleError::PathTooLong));
        return Result(true);
    }

    StringView components[] = {StringView(currentDirectory), StringView(argument)};
    SC_TRY(AsyncFibersFileCopyCheck(Path::join(path, components), FileCopyExampleError::PathTooLong));
    return Result(true);
}

static bool areSameFile(const FileDescriptorStat& first, const FileDescriptorStat& second)
{
#if SC_PLATFORM_WINDOWS
    return first.windows.volumeSerialNumber == second.windows.volumeSerialNumber and
           first.windows.fileIndex == second.windows.fileIndex;
#else
    return first.posix.device == second.posix.device and first.posix.inode == second.posix.inode;
#endif
}

struct AsyncFibersFileCopyState
{
    AsyncFiberIO*   io          = nullptr;
    FileDescriptor* source      = nullptr;
    FileDescriptor* destination = nullptr;
    Span<char>      buffer;
    size_t          copiedBytes = 0;

    //! [AsyncFibersFileCopyLoop]
    Result copy()
    {
        for (;;)
        {
            AsyncFiberFileReadResult readResult;
            SC_TRY(io->fileReadAt(*source, copiedBytes, buffer, readResult));

            if (not readResult.data.empty())
            {
                AsyncFiberFileWriteResult writeResult;
                SC_TRY(io->fileWriteAllAt(*destination, copiedBytes, readResult.data, &writeResult));
                SC_TRY(AsyncFibersFileCopyCheck(writeResult.numBytes == readResult.data.sizeInBytes(),
                                                FileCopyExampleError::ShortWrite));
                copiedBytes += writeResult.numBytes;
            }
            else if (not readResult.endOfFile)
            {
                return AsyncFibersFileCopyFailure(FileCopyExampleError::ReadNoProgress);
            }

            if (readResult.endOfFile)
            {
                return Result(true);
            }
        }
    }
    //! [AsyncFibersFileCopyLoop]
};

static Result runAsyncFibersFileCopy(int argc, const char* const* argv)
{
    StringSpan sourceArgument;
    StringSpan destinationArgument;

    CommandLinePositional positionals[2];
    positionals[0].name  = "source";
    positionals[0].help  = "Source file path";
    positionals[0].value = CommandLineValue::stringSpan(sourceArgument);
    positionals[1].name  = "destination";
    positionals[1].help  = "Destination file path";
    positionals[1].value = CommandLineValue::stringSpan(destinationArgument);

    CommandLineSpec spec;
    spec.programName = "AsyncFibersFileCopy";
    spec.summary     = "Copy one file using a bounded stackful-fiber I/O loop.";
    spec.positionals = positionals;

    StringSpan           argumentStorage[8];
    CommandLineArguments arguments;
    SC_TRY(arguments.setFromMainArguments(argc, argv, argumentStorage));
    const CommandLineParseResult parseResult = spec.parse(arguments.values);

    Console console;
    Console::tryAttachingToParentConsole();
    if (parseResult.status == CommandLineParseResult::Status::HelpRequested)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, true);
        SC_TRY(AsyncFibersFileCopyCheck(spec.writeHelp(output), FileCopyExampleError::HelpWriteFailed));
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, false);
        SC_TRY(AsyncFibersFileCopyCheck(spec.writeError(parseResult, output),
                                        FileCopyExampleError::ParseErrorWriteFailed));
        return AsyncFibersFileCopyFailure(FileCopyExampleError::InvalidArguments);
    }

    StringPath       currentDirectoryStorage;
    const StringSpan currentDirectory = FileSystem::Operations::getCurrentWorkingDirectory(currentDirectoryStorage);
    SC_TRY(AsyncFibersFileCopyCheck(not currentDirectory.isEmpty(), FileCopyExampleError::CurrentDirectoryUnavailable));

    StringPath sourcePath;
    StringPath destinationPath;
    SC_TRY(resolvePath(sourceArgument, currentDirectory, sourcePath));
    SC_TRY(resolvePath(destinationArgument, currentDirectory, destinationPath));
    SC_TRY(AsyncFibersFileCopyCheck(sourcePath.view() != destinationPath.view(), FileCopyExampleError::SamePath));

    AsyncEventLoop eventLoop;
    SC_TRY(eventLoop.create());
    auto closeEventLoop = MakeDeferred([&eventLoop] { (void)eventLoop.close(); });

    FileOpen sourceOpen(FileOpen::Read);
    sourceOpen.blocking = false;
    FileDescriptor source;
    SC_TRY(source.open(sourcePath.view(), sourceOpen));
    auto               closeSource = MakeDeferred([&source] { (void)source.close(); });
    FileDescriptorStat sourceStat;
    SC_TRY(source.stat(sourceStat));
    SC_TRY(AsyncFibersFileCopyCheck(sourceStat.entryType == FileDescriptorEntryType::File,
                                    FileCopyExampleError::SourceNotFile));
    SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(source));

    // AppendRead creates a missing destination without truncating an existing alias of the source.
    FileOpen       destinationInspectionOpen(FileOpen::AppendRead);
    FileDescriptor destination;
    SC_TRY(destination.open(destinationPath.view(), destinationInspectionOpen));
    auto               closeDestination = MakeDeferred([&destination] { (void)destination.close(); });
    FileDescriptorStat destinationStat;
    SC_TRY(destination.stat(destinationStat));
    SC_TRY(AsyncFibersFileCopyCheck(destinationStat.entryType == FileDescriptorEntryType::File,
                                    FileCopyExampleError::DestinationNotFile));
    SC_TRY(AsyncFibersFileCopyCheck(not areSameFile(sourceStat, destinationStat), FileCopyExampleError::SameFile));
    SC_TRY(destination.close());

    FileOpen destinationOpen(FileOpen::Write);
    destinationOpen.blocking = false;
    SC_TRY(destination.open(destinationPath.view(), destinationOpen));
    SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(destination));

    static constexpr size_t BufferSize             = 64 * 1024;
    char                    buffer[BufferSize]     = {};
    char                    stackMemory[64 * 1024] = {};
    FiberStack              stack(stackMemory);
    FiberTask               task;
    FiberScheduler          scheduler;
    AsyncFiberIO            io(scheduler, eventLoop);

    AsyncFibersFileCopyState state;
    state.io          = &io;
    state.source      = &source;
    state.destination = &destination;
    state.buffer      = buffer;

    AsyncFibersFileCopyState* statePointer = &state;
    SC_TRY(scheduler.spawn(task, stack,
                           FiberTask::Procedure([statePointer](FiberScheduler&) { return statePointer->copy(); })));
    SC_TRY(io.runUntilComplete());
    SC_TRY(task.result());

    console.print("Copied {} bytes from {} to {}\n", state.copiedBytes, sourcePath.view(), destinationPath.view());
    return Result(true);
}
} // namespace SC

int main(int argc, const char* const* argv)
{
    const SC::Result result = SC::runAsyncFibersFileCopy(argc, argv);
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256];
        const SC::ResultErrorFormat formatted = SC::formatAsyncFibersFileCopyError(result, message);
        if (formatted)
            console.print("AsyncFibersFileCopy failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AsyncFibersFileCopy failed: error category {}, code {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
