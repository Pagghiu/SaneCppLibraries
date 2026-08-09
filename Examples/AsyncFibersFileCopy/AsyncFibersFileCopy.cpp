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

namespace SC
{
static Result resolvePath(StringSpan argument, StringSpan currentDirectory, StringPath& path)
{
    if (Path::isAbsolute(StringView(argument), Path::AsNative))
    {
        SC_TRY_MSG(path.assign(argument), "AsyncFibersFileCopy path is too long");
        return Result(true);
    }

    StringView components[] = {StringView(currentDirectory), StringView(argument)};
    SC_TRY_MSG(Path::join(path, components), "AsyncFibersFileCopy path is too long");
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
                SC_TRY_MSG(writeResult.numBytes == readResult.data.sizeInBytes(),
                           "AsyncFibersFileCopy completed a short write");
                copiedBytes += writeResult.numBytes;
            }
            else if (not readResult.endOfFile)
            {
                return Result::Error("AsyncFibersFileCopy read made no progress");
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
        SC_TRY_MSG(spec.writeHelp(output), "Failed writing AsyncFibersFileCopy help");
        return Result(true);
    }
    if (parseResult.status == CommandLineParseResult::Status::Error)
    {
        StringFormatOutput output(StringEncoding::Utf8, console, false);
        SC_TRY_MSG(spec.writeError(parseResult, output), "Failed writing AsyncFibersFileCopy parse error");
        return Result::Error("Invalid AsyncFibersFileCopy arguments");
    }

    StringPath       currentDirectoryStorage;
    const StringSpan currentDirectory = FileSystem::Operations::getCurrentWorkingDirectory(currentDirectoryStorage);
    SC_TRY_MSG(not currentDirectory.isEmpty(), "AsyncFibersFileCopy could not resolve the current directory");

    StringPath sourcePath;
    StringPath destinationPath;
    SC_TRY(resolvePath(sourceArgument, currentDirectory, sourcePath));
    SC_TRY(resolvePath(destinationArgument, currentDirectory, destinationPath));
    SC_TRY_MSG(sourcePath.view() != destinationPath.view(), "AsyncFibersFileCopy source and destination must differ");

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
    SC_TRY_MSG(sourceStat.entryType == FileDescriptorEntryType::File, "AsyncFibersFileCopy source is not a file");
    SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(source));

    // AppendRead creates a missing destination without truncating an existing alias of the source.
    FileOpen       destinationInspectionOpen(FileOpen::AppendRead);
    FileDescriptor destination;
    SC_TRY(destination.open(destinationPath.view(), destinationInspectionOpen));
    auto               closeDestination = MakeDeferred([&destination] { (void)destination.close(); });
    FileDescriptorStat destinationStat;
    SC_TRY(destination.stat(destinationStat));
    SC_TRY_MSG(destinationStat.entryType == FileDescriptorEntryType::File,
               "AsyncFibersFileCopy destination is not a file");
    SC_TRY_MSG(not areSameFile(sourceStat, destinationStat),
               "AsyncFibersFileCopy source and destination identify the same file");
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
        console.print("AsyncFibersFileCopy failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
