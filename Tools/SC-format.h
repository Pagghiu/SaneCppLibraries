// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Libraries/Algorithms/AlgorithmFind.h"
#include "../Libraries/Async/Async.h"
#include "../Libraries/FileSystemIterator/FileSystemIterator.h"
#include "../Libraries/Process/Process.h"
#include "Tools.h"

namespace SC
{
namespace Tools
{
/// @brief Finds files recursively matching given extensions, excluding some directories and calls a function on them.
/// TODO: Maybe with some love this could be generalized and be added to some library
struct FileSystemFinder
{
    [[nodiscard]] static Result forEachFile(StringView libraryDirectory, Span<const StringView> includeFilesEndingWith,
                                            Span<const StringView>         excludeDirectories,
                                            Function<Result(StringView)>&& forEachFunc)
    {
        FileSystemIterator::FolderState entries[16];

        FileSystemIterator iterator;
        SC_TRY(iterator.init(libraryDirectory, entries));

        while (iterator.enumerateNext())
        {
            const StringView name = iterator.get().name;
            if (iterator.get().isDirectory())
            {
                if (not Algorithms::contains(excludeDirectories, name))
                {
                    SC_TRY(iterator.recurseSubdirectory());
                }
            }
            else
            {
                for (auto extension : includeFilesEndingWith)
                {
                    if (name.endsWith(extension))
                    {
                        SC_TRY(forEachFunc(iterator.get().path));
                        break;
                    }
                }
            }
        }
        return Result(true);
    }
};

/// @brief Launch processes obeying to a maximum predefined concurrency passed during create
/// TODO: Maybe with some love this could be generalized and be added to some library
struct ProcessLimiter
{
    /// @brief Create the process limiter with an hint of maximum number of processes to allow
    [[nodiscard]] Result create(size_t maxProcessesHint, Span<AsyncProcessExit> processExitPool)
    {
        processResult = Result(true);
        if (processExitPool.sizeInElements() < maxProcessesHint)
        {
            maxProcessesHint = processExitPool.sizeInElements();
        }
        processMonitors = processExitPool;
        for (size_t idx = 0; idx < maxProcessesHint; ++idx)
        {
            availableProcessMonitors.queueBack(processMonitors[idx]);
        }
        AsyncEventLoop::Options options;
#if SC_PLATFORM_LINUX
        // TODO: Investigate why ProcessLimiter fails on uring backend
        options.apiType = AsyncEventLoop::Options::ApiType::ForceUseEpoll;
#endif
        return eventLoop.create(options);
    }

    /// @brief Waits for any process still running and free the resources created by event loop
    /// @return Invalid result if any process returned non zero value
    [[nodiscard]] Result close()
    {
        SC_TRY(eventLoop.run()); // wait for outstanding tasks
        SC_TRY(eventLoop.close());
        return processResult;
    }

    /// @brief Launches a new background process eventually blocking until one slot becomes available
    [[nodiscard]] Result launch(Span<const StringSpan> arguments)
    {
        while (availableProcessMonitors.isEmpty())
        {
            // If there are no available processes just wait for one to finish
            SC_TRY(eventLoop.runOnce());
        }
        if (not processResult)
        {
            return processResult;
        }
        AsyncProcessExit& processExit = *availableProcessMonitors.dequeueFront();

        Process process;
        SC_TRY(process.launch(arguments));
        // Launch does not wait for the child process to finish so we can monitor it with the event loop
        processExit.callback = [this](AsyncProcessExit::Result& result)
        {
            int exitStatus = -1;
            processResult  = result.get(exitStatus);
            if (processResult and exitStatus != 0)
            {
                processResult = Result::Error("ProcessLimiter::callback - returned non zero");
            }
            // This child process has exited, let's make its slot available again
            availableProcessMonitors.queueBack(result.getAsync());
        };
        // Start monitoring process exit on the event loop
        ProcessDescriptor::Handle processHandle = 0;
        SC_TRY(process.handle.get(processHandle, Result::Error("Invalid Handle")));
        SC_TRY(processExit.start(eventLoop, processHandle));
        process.handle.detach(); // we can't close it
        return Result(true);
    }

  private:
    AsyncEventLoop eventLoop;

    Result processResult = Result(true);

    Span<AsyncProcessExit> processMonitors;

    IntrusiveDoubleLinkedList<AsyncProcessExit> availableProcessMonitors;
};
} // namespace Tools
} // namespace SC
